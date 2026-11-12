#include "chrome_render.h"

#include "util.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

std::string chrome_render::init(const std::string& repo, const std::string& cache_dir)
{
    m_ready.store(false);
    m_repo = fs::absolute(repo).lexically_normal().string();
    m_cache_dir = fs::absolute(cache_dir).lexically_normal().string();

    std::string fonts_out = (fs::path(m_cache_dir) / "fonts").string();
    std::string cmd = "python3 " + shell_quote(m_repo + "/check-with-chrome/scripts/gen_fonts_conf.py")
        + " --repo " + shell_quote(m_repo)
        + " --out " + shell_quote(fonts_out);
    std::string out;
    int rc = run_command(cmd, out);
    if (rc != 0)
        return "Failed to generate chrome fontconfig:\n" + tail(out);

    m_fonts_conf = (fs::path(fonts_out) / "fonts.conf").string();
    if (!fs::exists(m_fonts_conf))
        return "Chrome fontconfig not found after generation";

    m_ready.store(true);
    return "";
}

std::string chrome_render::render(const std::string& html, int width, int height, const std::string& out_png)
{
    if (!m_ready.load())
        return "chrome: not ready";

    int id = m_counter++;
    std::string jobs = (fs::path(m_cache_dir) / ("chrome-jobs-" + std::to_string(id) + ".tsv")).string();
    std::string errors = (fs::path(m_cache_dir) / ("chrome-errors-" + std::to_string(id) + ".txt")).string();
    std::string profile = (fs::path(m_cache_dir) / ("chrome-profile-" + std::to_string(id))).string();
    std::string home = (fs::path(m_cache_dir) / ("chrome-home-" + std::to_string(id))).string();

    {
        std::ofstream f(jobs);
        if (!f)
            return "chrome: cannot write jobs file";
        // std::to_string() is locale-independent, unlike stream << which would
        // apply a thousands separator under a non-C locale (breaking the TSV).
        f << html << "\t" << std::to_string(width) << "\t" << std::to_string(height) << "\t" << out_png << "\n";
    }

    std::string cmd = "python3 " + shell_quote(m_repo + "/check-with-chrome/scripts/chrome_capture.py")
        + " --list " + shell_quote(jobs)
        + " --errors " + shell_quote(errors)
        + " --fontconfig " + shell_quote(m_fonts_conf)
        + " --profile " + shell_quote(profile)
        + " --home " + shell_quote(home)
        + " --jobs 1";
    std::string out;
    int rc = run_command(cmd, out);

    // Gather any error detail before removing the temporary files.
    std::string error_message;
    if (rc != 0)
    {
        error_message = "chrome capture failed:\n" + tail(out);
        std::ifstream ef(errors);
        std::stringstream ss;
        ss << ef.rdbuf();
        std::string errs = ss.str();
        if (!errs.empty())
            error_message += "\n" + tail(errs);
    }

    // Per-render scratch files/dirs are no longer needed.
    std::error_code ec;
    fs::remove(jobs, ec);
    fs::remove(errors, ec);
    fs::remove_all(profile, ec);
    fs::remove_all(home, ec);

    if (rc != 0)
        return error_message;
    if (!fs::exists(out_png))
        return "chrome produced no output";
    return "";
}

#include "chrome_render.h"

#include "util.h"

#include <filesystem>

namespace fs = std::filesystem;

std::string chrome_render::init(const std::string& repo, const std::string& cache_dir)
{
    m_ready.store(false);
    m_repo = fs::absolute(repo).lexically_normal().string();
    m_cache_dir = fs::absolute(cache_dir).lexically_normal().string();

    if (!fs::exists(fs::path(m_repo) / "render.py"))
        return "render.py not found under " + m_repo;
    if (!fs::is_directory(fs::path(m_repo) / "fonts"))
        return "fonts directory not found under " + m_repo;

    m_ready.store(true);
    return "";
}

std::string chrome_render::render(const std::string& html, const std::string& out_png)
{
    if (!m_ready.load())
        return "chrome: not ready";

    // render.py applies the same bestfit + .cfg width logic as render2png
    // (starting from render2png's 800px first pass) and generates its own
    // fontconfig from the repository fonts/.
    std::string fonts_out = (fs::path(m_cache_dir) / "chrome-fonts").string();
    std::string cmd = "python3 " + shell_quote(m_repo + "/render.py")
        + " " + shell_quote(html)
        + " -o " + shell_quote(out_png)
        + " -w 800"
        + " --repo " + shell_quote(m_repo)
        + " --fonts-out " + shell_quote(fonts_out);
    std::string out;
    int rc = run_command(cmd, out);
    if (rc != 0)
        return "chrome capture failed:\n" + tail(out);
    if (!fs::exists(out_png))
        return "chrome produced no output";
    return "";
}

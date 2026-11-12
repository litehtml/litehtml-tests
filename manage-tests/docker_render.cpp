#include "docker_render.h"

#include "util.h"

#include <filesystem>
#include <unistd.h>

namespace fs = std::filesystem;

std::string docker_render::init(const std::string& tests_folder, const std::string& litehtml_folder,
                                const std::string& repo, const std::string& cache_dir)
{
    m_ready.store(false);
    m_tests_folder = fs::absolute(tests_folder).lexically_normal().string();
    m_cache_dir = fs::absolute(cache_dir).lexically_normal().string();
    std::string repo_abs = fs::absolute(repo).lexically_normal().string();
    std::string litehtml_abs = fs::absolute(litehtml_folder).lexically_normal().string();

    std::string out;

    // 1. docker available?
    if (run_command("docker version", out) != 0)
        return "Docker is not available or not running";

    // 2. image exists?
    const std::string image = "litehtml-build:latest";
    run_command("docker images -q " + image, out);
    bool image_exists = !out.empty() && out.find_first_not_of(" \t\n\r") != std::string::npos;

    if (!image_exists)
    {
        std::string cmd = "docker build -t " + image + " " + shell_quote(repo_abs);
        int rc = run_command(cmd, out);
        if (rc != 0)
            return "Failed to build docker image " + image + ":\n" + tail(out);
    }

    // 3. container name + directories
    m_container_name = make_container_name();
    std::string build_dir = (fs::path(m_cache_dir) / "docker-build").string();
    fs::create_directories(build_dir);
    fs::create_directories(fs::path(m_cache_dir) / "render2png");

    // 4. remove any stale container with the same name
    run_command("docker rm -f " + shell_quote(m_container_name), out);

    // 5. start the persistent container (run as the host user so outputs are owned
    //    by the user and can be renamed/copied by the app later)
    std::string run_cmd = "docker run -d --name " + shell_quote(m_container_name)
        + " -u $(id -u):$(id -g)"
        + " -v " + shell_quote(litehtml_abs) + ":/litehtml:ro"
        + " -v " + shell_quote(repo_abs) + ":/litehtml-tests:ro"
        + " -v " + shell_quote(m_tests_folder) + ":/tests:ro"
        + " -v " + shell_quote(build_dir) + ":/build"
        + " -v " + shell_quote(m_cache_dir) + ":/cache"
        + " " + image + " sleep infinity";
    int rc = run_command(run_cmd, out);
    if (rc != 0)
        return "Failed to start docker container:\n" + tail(out);

    // 6. configure the render2png build
    std::string cfg_cmd = "docker exec " + shell_quote(m_container_name)
        + " cmake -DLITEHTML_PATH=/litehtml -DLITEHTML_BUILD_RENDER2PNG=ON -DEXTERNAL_GTEST=ON"
        + " -B /build -S /litehtml-tests";
    rc = run_command(cfg_cmd, out);
    if (rc != 0)
        return "Failed to configure render2png build:\n" + tail(out);

    // 7. build render2png
    std::string build_cmd = "docker exec " + shell_quote(m_container_name)
        + " cmake --build /build --target render2png -j $(nproc)";
    rc = run_command(build_cmd, out);
    if (rc != 0)
        return "Failed to build render2png:\n" + tail(out);

    m_ready.store(true);
    return "";
}

std::string docker_render::render(const std::string& html, const std::string& out_png)
{
    if (!m_ready.load())
        return "render2png: docker not ready";

    std::error_code ec;
    std::string rel_html = fs::relative(html, m_tests_folder, ec).string();
    if (ec)
        return "render2png: cannot compute relative path for " + html;
    std::string rel_out = fs::relative(out_png, m_cache_dir, ec).string();
    if (ec)
        return "render2png: cannot compute relative output path";

    std::string cmd = "docker exec " + shell_quote(m_container_name)
        + " /build/render2png " + shell_quote("/tests/" + rel_html)
        + " " + shell_quote("/cache/" + rel_out);
    std::string out;
    int rc = run_command(cmd, out);
    if (rc != 0)
        return "render2png failed:\n" + tail(out);
    if (!fs::exists(out_png))
        return "render2png produced no output";
    return "";
}

void docker_render::shutdown()
{
    if (m_container_name.empty())
        return;
    std::string out;
    run_command("docker rm -f " + shell_quote(m_container_name), out);
    m_container_name.clear();
    m_ready.store(false);
}

std::string docker_render::make_container_name()
{
    return "litehtml-manage-tests-" + std::to_string(::getpid());
}

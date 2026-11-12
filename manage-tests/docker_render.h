#ifndef MANAGE_TESTS_DOCKER_RENDER_H
#define MANAGE_TESTS_DOCKER_RENDER_H

#include <atomic>
#include <string>

class docker_render
{
public:
    // Ensure the docker image exists, start a persistent container and build
    // render2png inside it. Returns an error message (empty on success).
    std::string init(const std::string& tests_folder, const std::string& litehtml_folder,
                     const std::string& repo, const std::string& cache_dir);

    // Render `html` (absolute host path) to `out_png` (absolute host path under
    // cache_dir) using `docker exec`. Returns an error message (empty on success).
    std::string render(const std::string& html, const std::string& out_png);

    void shutdown();

    bool ready() const { return m_ready.load(); }

    // Deterministic, pid-based name used for the persistent container.
    static std::string make_container_name();

private:
    std::string m_tests_folder;
    std::string m_cache_dir;
    std::string m_container_name;
    std::atomic<bool> m_ready{false};
};

#endif // MANAGE_TESTS_DOCKER_RENDER_H

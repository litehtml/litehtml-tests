#ifndef MANAGE_TESTS_CHROME_RENDER_H
#define MANAGE_TESTS_CHROME_RENDER_H

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <sys/types.h>

// Renders Chrome comparison images through a persistent `render.py --serve`
// process, so Python and Chrome start once instead of once per image.
class chrome_render
{
public:
    ~chrome_render();

    // Verify render.py and the repository fonts/ are available. Returns an
    // error message (empty on success).
    std::string init(const std::string& repo, const std::string& cache_dir);

    // Render `html` in Chrome via render.py (bestfit + .cfg), writing the
    // full-page screenshot to `out_png`. Returns error message (empty on success).
    std::string render(const std::string& html, const std::string& out_png);

    // Stop the render.py server (it is restarted on the next render()).
    void shutdown();

    bool ready() const { return m_ready.load(); }

private:
    std::string start_server();
    void stop_server();
    std::string request(const std::string& line, std::string& reply);
    std::string server_log_tail() const;

    std::string m_repo;
    std::string m_cache_dir;
    std::atomic<bool> m_ready{false};

    std::mutex m_mutex;
    pid_t m_pid = -1;
    int m_to_server = -1;
    FILE* m_from_server = nullptr;
};

#endif // MANAGE_TESTS_CHROME_RENDER_H

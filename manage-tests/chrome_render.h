#ifndef MANAGE_TESTS_CHROME_RENDER_H
#define MANAGE_TESTS_CHROME_RENDER_H

#include <atomic>
#include <string>

class chrome_render
{
public:
    // Verify render.py and the repository fonts/ are available. Returns an
    // error message (empty on success).
    std::string init(const std::string& repo, const std::string& cache_dir);

    // Render `html` in Chrome via render.py (bestfit + .cfg), writing the
    // full-page screenshot to `out_png`. Returns error message (empty on success).
    std::string render(const std::string& html, const std::string& out_png);

    bool ready() const { return m_ready.load(); }

private:
    std::string m_repo;
    std::string m_cache_dir;
    std::atomic<bool> m_ready{false};
};

#endif // MANAGE_TESTS_CHROME_RENDER_H

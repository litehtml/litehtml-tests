#ifndef MANAGE_TESTS_CHROME_RENDER_H
#define MANAGE_TESTS_CHROME_RENDER_H

#include <atomic>
#include <string>

class chrome_render
{
public:
    // Generate the fontconfig used by Chrome. Returns error message (empty on success).
    std::string init(const std::string& repo, const std::string& cache_dir);

    // Render `html` in Chrome at the given viewport width/height, writing the
    // full-page screenshot to `out_png`. Returns error message (empty on success).
    std::string render(const std::string& html, int width, int height, const std::string& out_png);

    bool ready() const { return m_ready.load(); }

private:
    std::string m_repo;
    std::string m_cache_dir;
    std::string m_fonts_conf;
    int m_counter = 0;
    std::atomic<bool> m_ready{false};
};

#endif // MANAGE_TESTS_CHROME_RENDER_H

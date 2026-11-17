#include "chrome_render.h"

#include "util.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace fs = std::filesystem;

namespace
{

std::string json_string(const std::string& s)
{
    std::string r = "\"";
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '"': r += "\\\""; break;
        case '\\': r += "\\\\"; break;
        case '\n': r += "\\n"; break;
        case '\r': r += "\\r"; break;
        case '\t': r += "\\t"; break;
        default:
            if (c < 0x20)
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                r += buf;
            }
            else
                r += static_cast<char>(c);
        }
    }
    return r + "\"";
}

// Extract the "error" string from a render.py reply (ASCII-escaped JSON).
std::string json_error(const std::string& reply)
{
    auto pos = reply.find("\"error\": \"");
    if (pos == std::string::npos)
        return reply;
    std::string r;
    for (size_t i = pos + 10; i < reply.size() && reply[i] != '"'; i++)
    {
        if (reply[i] != '\\' || i + 1 >= reply.size())
        {
            r += reply[i];
            continue;
        }
        char e = reply[++i];
        if (e == 'n')
            r += '\n';
        else if (e == 't')
            r += '\t';
        else if (e == 'u' && i + 4 < reply.size())
        {
            unsigned long cp = std::strtoul(reply.substr(i + 1, 4).c_str(), nullptr, 16);
            r += cp < 0x80 ? static_cast<char>(cp) : '?';
            i += 4;
        }
        else
            r += e;
    }
    return r;
}

bool write_all(int fd, const std::string& data)
{
    size_t done = 0;
    while (done < data.size())
    {
        ssize_t n = ::write(fd, data.data() + done, data.size() - done);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return false;
        done += static_cast<size_t>(n);
    }
    return true;
}

} // namespace

chrome_render::~chrome_render()
{
    shutdown();
}

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

void chrome_render::shutdown()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    stop_server();
}

std::string chrome_render::server_log_tail() const
{
    std::ifstream f(fs::path(m_cache_dir) / "chrome-server.log");
    std::stringstream ss;
    ss << f.rdbuf();
    return tail(ss.str());
}

std::string chrome_render::start_server()
{
    // A dead server must surface as a write error, not kill the whole app.
    std::signal(SIGPIPE, SIG_IGN);

    std::error_code ec;
    fs::create_directories(m_cache_dir, ec);

    // render.py applies the same bestfit + .cfg width logic as render2png
    // (starting from render2png's 800px first pass) and generates its own
    // fontconfig from the repository fonts/.
    std::vector<std::string> args = {
        "python3", m_repo + "/render.py", "--serve",
        "-w", "800",
        "--repo", m_repo,
        "--fonts-out", (fs::path(m_cache_dir) / "chrome-fonts").string(),
    };
    std::vector<char*> argv;
    for (auto& a : args)
        argv.push_back(a.data());
    argv.push_back(nullptr);

    int to_child[2];
    int from_child[2];
    if (pipe2(to_child, O_CLOEXEC) != 0)
        return "chrome: pipe failed";
    if (pipe2(from_child, O_CLOEXEC) != 0)
    {
        close(to_child[0]);
        close(to_child[1]);
        return "chrome: pipe failed";
    }
    std::string log = (fs::path(m_cache_dir) / "chrome-server.log").string();
    int log_fd = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, to_child[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, from_child[1], STDOUT_FILENO);
    if (log_fd >= 0)
        posix_spawn_file_actions_adddup2(&actions, log_fd, STDERR_FILENO);

    pid_t pid = -1;
    int rc = posix_spawnp(&pid, "python3", &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(to_child[0]);
    close(from_child[1]);
    if (log_fd >= 0)
        close(log_fd);
    if (rc != 0)
    {
        close(to_child[1]);
        close(from_child[0]);
        return std::string("chrome: failed to start python3: ") + std::strerror(rc);
    }

    m_pid = pid;
    m_to_server = to_child[1];
    m_from_server = fdopen(from_child[0], "r");
    return "";
}

void chrome_render::stop_server()
{
    if (m_to_server >= 0)
    {
        close(m_to_server);
        m_to_server = -1;
    }
    if (m_from_server)
    {
        fclose(m_from_server);
        m_from_server = nullptr;
    }
    if (m_pid > 0)
    {
        // EOF on stdin makes the server close Chrome and exit; force it if it hangs.
        for (int i = 0; i < 50; i++)
        {
            if (waitpid(m_pid, nullptr, WNOHANG) != 0)
            {
                m_pid = -1;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        kill(m_pid, SIGKILL);
        waitpid(m_pid, nullptr, 0);
        m_pid = -1;
    }
}

// Send one request line and read the reply line. Returns an error (empty on
// success) if the server could not be reached.
std::string chrome_render::request(const std::string& line, std::string& reply)
{
    if (m_pid <= 0)
    {
        std::string err = start_server();
        if (!err.empty())
            return err;
    }
    if (!write_all(m_to_server, line))
        return "chrome: render.py server is not running";

    reply.clear();
    char buf[4096];
    while (fgets(buf, sizeof(buf), m_from_server))
    {
        reply += buf;
        if (!reply.empty() && reply.back() == '\n')
            return "";
    }
    return "chrome: render.py server exited";
}

std::string chrome_render::render(const std::string& html, const std::string& out_png)
{
    if (!m_ready.load())
        return "chrome: not ready";

    std::string line = "{\"source\": " + json_string(html)
        + ", \"output\": " + json_string(out_png)
        + ", \"width\": 800}\n";

    std::lock_guard<std::mutex> lk(m_mutex);
    std::string reply;
    std::string err = request(line, reply);
    if (!err.empty())
    {
        // The server died; restart it once and retry.
        stop_server();
        err = request(line, reply);
        if (!err.empty())
        {
            std::string log = server_log_tail();
            stop_server();
            return err + (log.empty() ? "" : ":\n" + log);
        }
    }
    if (reply.find("\"ok\": true") == std::string::npos)
        return "chrome capture failed:\n" + json_error(reply);
    if (!fs::exists(out_png))
        return "chrome produced no output";
    return "";
}

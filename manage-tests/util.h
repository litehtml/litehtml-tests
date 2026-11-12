#ifndef MANAGE_TESTS_UTIL_H
#define MANAGE_TESTS_UTIL_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/wait.h>

// Quote a string for safe use as a single shell argument.
inline std::string shell_quote(const std::string& s)
{
    std::string r = "'";
    for (char c : s)
    {
        if (c == '\'')
            r += "'\\''";
        else
            r += c;
    }
    r += '\'';
    return r;
}

// Run a shell command and capture its combined stdout+stderr into `out`.
// Returns the process exit code, or -1 if it could not be started.
inline int run_command(const std::string& cmd, std::string& out)
{
    out.clear();
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe)
        return -1;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe))
        out += buf;
    int rc = pclose(pipe);
    if (rc == -1)
        return -1;
    if (WIFEXITED(rc))
        return WEXITSTATUS(rc);
    return -1;
}

inline int run_command(const std::string& cmd)
{
    std::string out;
    return run_command(cmd, out);
}

// Last `n` characters of `s` (for concise error messages).
inline std::string tail(const std::string& s, size_t n = 2000)
{
    if (s.size() <= n)
        return s;
    return s.substr(s.size() - n);
}

// Read the width/height from a PNG file header (IHDR chunk).
inline bool read_png_size(const std::string& path, int& w, int& h)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;
    unsigned char sig[8];
    f.read(reinterpret_cast<char*>(sig), 8);
    static const unsigned char png_sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (!f || std::memcmp(sig, png_sig, 8) != 0)
        return false;
    unsigned char len[4];
    unsigned char type[4];
    f.read(reinterpret_cast<char*>(len), 4);
    f.read(reinterpret_cast<char*>(type), 4);
    if (!f || std::memcmp(type, "IHDR", 4) != 0)
        return false;
    unsigned char wh[8];
    f.read(reinterpret_cast<char*>(wh), 8);
    if (!f)
        return false;
    auto be = [](const unsigned char* p) { return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; };
    w = be(wh);
    h = be(wh + 4);
    return true;
}

#endif // MANAGE_TESTS_UTIL_H

#include "test_model.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

std::string test_entry::strip_leading_dashes(const std::string& path)
{
    fs::path p(path);
    std::string fname = p.filename().string();
    size_t i = 0;
    while (i < fname.size() && fname[i] == '-')
        i++;
    std::string stripped = fname.substr(i);
    return (p.parent_path() / stripped).string();
}

std::string test_entry::status() const
{
    switch (action)
    {
        case test_action::ignore:
            return "ignore";
        case test_action::approve:
            return "approve";
        case test_action::disable:
            return "disable";
    }
    return "unknown";
}

namespace test_model
{

namespace
{
    bool is_html(const std::string& name)
    {
        auto ext = fs::path(name).extension().string();
        return ext == ".html" || ext == ".htm";
    }

    bool starts_with_dash(const std::string& name)
    {
        return !name.empty() && name[0] == '-';
    }

    void scan_recursive(const fs::path& dir, test_mode mode, std::vector<std::string>& out)
    {
        std::error_code ec;
        fs::directory_iterator it(dir, ec);
        if (ec)
            return;
        for (const auto& entry : it)
        {
            std::string name = entry.path().filename().string();
            if (entry.is_directory())
            {
                if (name == "support")
                    continue;
                if (mode != test_mode::disabled && starts_with_dash(name))
                    continue;
                scan_recursive(entry.path(), mode, out);
            }
            else if (entry.is_regular_file())
            {
                if (!is_html(name))
                    continue;
                if (mode == test_mode::disabled)
                {
                    if (starts_with_dash(name))
                        out.push_back(entry.path().string());
                }
                else
                {
                    if (starts_with_dash(name))
                        continue;
                    std::string base = test_entry::strip_leading_dashes(entry.path().string());
                    bool has_failed = fs::exists(base + "-FAILED.png");
                    if (mode == test_mode::failed && has_failed)
                        out.push_back(entry.path().string());
                    else if (mode == test_mode::passed && !has_failed)
                        out.push_back(entry.path().string());
                }
            }
        }
    }
}

std::vector<std::string> scan(const std::string& tests_folder, test_mode mode)
{
    std::vector<std::string> out;
    if (!fs::exists(tests_folder))
        return out;
    scan_recursive(fs::path(tests_folder), mode, out);
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace test_model

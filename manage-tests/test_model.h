#ifndef MANAGE_TESTS_TEST_MODEL_H
#define MANAGE_TESTS_TEST_MODEL_H

#include <string>
#include <vector>

enum class test_action { ignore, approve, disable };

enum class test_mode { failed, passed, disabled };

struct test_entry
{
    std::string html; // path to the html file on disk (may start with '-')
    std::string base; // html with leading '-' stripped from the filename
    test_action action;
    bool approve_from_render2png;

    explicit test_entry(std::string _html)
        : html(std::move(_html)), action(test_action::ignore), approve_from_render2png(false)
    {
        base = strip_leading_dashes(html);
    }

    std::string status() const;
    std::string golden_path() const { return base + ".png"; }
    std::string failed_path() const { return base + "-FAILED.png"; }

    static std::string strip_leading_dashes(const std::string& path);
};

namespace test_model
{
    // Scan `tests_folder` for the given mode and return a sorted list of html paths.
    std::vector<std::string> scan(const std::string& tests_folder, test_mode mode);
}

#endif // MANAGE_TESTS_TEST_MODEL_H

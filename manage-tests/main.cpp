#include <gtkmm.h>

#include <iostream>
#include <string>
#include <vector>

#include "main_window.h"

static void print_help()
{
    std::cout << "manage-tests - triage litehtml golden-image render tests\n"
              << "\n"
              << "Usage:\n"
              << "  manage-tests [OPTIONS] [tests_folder] [litehtml_folder]\n"
              << "\n"
              << "Arguments:\n"
              << "  tests_folder    Folder with tests to scan (default: render)\n"
              << "  litehtml_folder Folder with the litehtml checkout, required to\n"
              << "                  build render2png inside Docker (default: ../litehtml)\n"
              << "\n"
              << "Options:\n"
              << "  -h, --help      Show this help and exit\n";
}

int main(int argc, char* argv[])
{
    std::string tests_folder = "render";
    std::string litehtml_folder = "../litehtml";
    std::vector<std::string> positional;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_help();
            return 0;
        }
        if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "Unknown option: " << arg << "\n\n";
            print_help();
            return 1;
        }
        positional.push_back(arg);
    }

    if (positional.size() > 2)
    {
        std::cerr << "Too many arguments\n\n";
        print_help();
        return 1;
    }
    if (positional.size() >= 1)
        tests_folder = positional[0];
    if (positional.size() >= 2)
        litehtml_folder = positional[1];

    // GtkApplication would interpret leftover positional arguments as files to
    // open, so only pass the program name to the application itself.
    char* gtk_argv[] = {argv[0]};
    auto app = Gtk::Application::create("litehtml.manage-tests");
    return app->make_window_and_run<MainWindow>(1, gtk_argv, tests_folder, litehtml_folder);
}

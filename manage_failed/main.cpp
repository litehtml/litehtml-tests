#include <gtkmm.h>
#include <string>
#include "main_window.h"

int main(int argc, char* argv[])
{
	auto app = Gtk::Application::create("litehtml.failchecker");

	std::string path;
	if(argc > 1)
	{
		path = argv[1];
	}

	return app->make_window_and_run<MainWindow>(argc, argv, path);
}

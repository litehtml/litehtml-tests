#include <gtkmm.h>
#include <gtkmm/application.h>
#include <string>
#include "main_window.h"

int on_cmd(const Glib::RefPtr<Gio::ApplicationCommandLine> &, Glib::RefPtr<Gtk::Application> &app)
{
	app->activate();
	return 0;
}
int main (int argc, char *argv[])
{
	Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(argc, argv, "litehtml.failchecker", Gio::APPLICATION_HANDLES_COMMAND_LINE);

	app->signal_command_line().connect(
			sigc::bind(sigc::ptr_fun(on_cmd), app), false);

	std::string path;
	if(argc > 1)
	{
		path = argv[1];
	}
	MainWindow win(path);
	return app->run(win);
}

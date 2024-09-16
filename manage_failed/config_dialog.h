#ifndef LITEHTML_TESTSUITE_CONFIG_DIALOG_H
#define LITEHTML_TESTSUITE_CONFIG_DIALOG_H

#include <gtkmm/dialog.h>
#include <gtkmm/textview.h>
#include <gtkmm/scrolledwindow.h>

class html_config_dialog : public Gtk::Dialog
{
	Gtk::TextView		m_text_view;
	Gtk::ScrolledWindow m_scrolled_wnd;
	Gtk::Button			m_btnSave;
	std::string 		m_cfg_file;
public:
	html_config_dialog(Gtk::Window& parent, const std::string& html_path);

private:
	std::string load_config();
	bool save_config();
	void on_btnSave_clicked();
};

#endif //LITEHTML_TESTSUITE_CONFIG_DIALOG_H

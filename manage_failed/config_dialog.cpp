#include <filesystem>
#include <fstream>
#include <sstream>
#include "config_dialog.h"

namespace fs = std::filesystem;

html_config_dialog::html_config_dialog(Gtk::Window &parent, const std::string &html_path) :
		Gtk::Dialog("HTML File Config", parent, true, true),
		m_cfg_file(html_path + ".cfg")
{
	auto cfg_text = load_config();

	add_button("Close", Gtk::ResponseType::CLOSE);

	get_header_bar()->pack_start(m_btnSave);
	m_btnSave.set_label("Save");
	m_btnSave.signal_clicked().connect( sigc::mem_fun(*this, &html_config_dialog::on_btnSave_clicked) );

	m_scrolled_wnd.set_hexpand(true);
	m_scrolled_wnd.set_vexpand(true);
	get_content_area()->append(m_scrolled_wnd);
	m_scrolled_wnd.set_child(m_text_view);
	m_text_view.set_editable(true);
	m_text_view.set_monospace(true);
	m_text_view.get_buffer()->set_text(cfg_text);

	set_default_size(400, 300);
}

std::string html_config_dialog::load_config()
{
	fs::path cfg_path(m_cfg_file);
	if(exists(cfg_path))
	{
		std::ifstream infile(cfg_path);
		if(infile.is_open())
		{
			std::stringstream buffer;
			buffer << infile.rdbuf();
			return buffer.str();
		}
	}
	return {};
}

bool html_config_dialog::save_config()
{
	bool saved = false;
	try
	{
		fs::path cfg_path(m_cfg_file);
		std::ofstream outfile(cfg_path, std::ios::out | std::ios::trunc);
		if (outfile.is_open())
		{
			outfile << m_text_view.get_buffer()->get_text();
			saved = true;
		}
	} catch(...) {}
	if(!saved)
	{
		auto dlg = Gtk::AlertDialog::create("ERROR: Impossible to save file");
		dlg->show(*this);
	}
	return saved;
}

void html_config_dialog::on_btnSave_clicked()
{
	if(save_config())
	{
		response(Gtk::ResponseType::APPLY);
	}
}

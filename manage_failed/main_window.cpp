#include "main_window.h"
#include <filesystem>
#include <sstream>
#include <iostream>
#include <unistd.h>
#include <linux/limits.h>
#include <giomm/appinfo.h>
#include "config_dialog.h"

namespace fs = std::filesystem;

namespace
{
	void set_tool_button(Gtk::Button& button, const std::string& icon_name, const std::string& label)
	{
		auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);
		auto icon = Gtk::make_managed<Gtk::Image>();
		icon->set_from_icon_name(icon_name);
		auto text = Gtk::make_managed<Gtk::Label>(label);
		box->append(*icon);
		box->append(*text);
		button.set_child(*box);
	}
}

MainWindow::MainWindow(const std::string& path) : m_path(path), m_current_test(-1)
{
	set_titlebar(m_header);

	m_header.set_show_title_buttons(true);

	m_header.pack_start(m_btnCfg);
	m_btnCfg.signal_clicked().connect( sigc::mem_fun(*this, &MainWindow::on_btnCfg_clicked) );
	set_tool_button(m_btnCfg, "accessories-text-editor-symbolic", "Config");

	m_header.pack_start(m_btnBack);
	m_btnBack.signal_clicked().connect( sigc::mem_fun(*this, &MainWindow::on_btnBack_clicked) );
	set_tool_button(m_btnBack, "media-skip-backward-symbolic", "Back");

	m_header.pack_start(m_btnNext);
	m_btnNext.signal_clicked().connect( sigc::mem_fun(*this, &MainWindow::on_btnNext_clicked) );
	set_tool_button(m_btnNext, "media-skip-forward-symbolic", "Next");

	m_header.pack_start(m_sep);

	m_header.pack_start(m_btnApprove);
	m_btnApprove.signal_clicked().connect( sigc::mem_fun(*this, &MainWindow::on_btnApprove_clicked) );
	set_tool_button(m_btnApprove, "emblem-default-symbolic", "Approve");

	m_header.pack_start(m_btnIgnore);
	m_btnIgnore.signal_clicked().connect( sigc::mem_fun(*this, &MainWindow::on_btnIgnore_clicked) );
	set_tool_button(m_btnIgnore, "media-seek-forward-symbolic", "Ignore");

	m_header.pack_start(m_btnDisable);
	m_btnDisable.signal_clicked().connect( sigc::mem_fun(*this, &MainWindow::on_btnDisable_clicked) );
	set_tool_button(m_btnDisable, "off-symbolic", "Disable");

	m_header.pack_end(m_btnSave);
	m_btnSave.signal_clicked().connect( sigc::mem_fun(*this, &MainWindow::on_btnSave_clicked) );
	set_tool_button(m_btnSave, "document-save-symbolic", "Save");

	m_header.pack_end(m_btnBrowser);
	m_btnBrowser.signal_clicked().connect( sigc::mem_fun(*this, &MainWindow::on_btnBrowser_clicked) );
	set_tool_button(m_btnBrowser, "network-workgroup-symbolic", "Browser");

	m_header.set_title_widget(m_labelTitle);
	m_labelTitle.set_hexpand(true);

	set_child(m_vbox);

	m_vbox.append(m_labelFile);

	m_vbox.append(m_hbox);

	m_hbox.append(m_frameOK);
	m_frameOK.set_hexpand(true);
	m_frameOK.set_label_align(0.5f);

	m_frameOK.set_child(m_vbox_ok);
	m_vbox_ok.append(m_image);
	m_image.set_margin_top(10);
	m_image.set_margin_bottom(10);
	m_image.set_margin_start(10);
	m_image.set_margin_end(10);
	m_image.set_halign(Gtk::Align::END);
	m_image.set_valign(Gtk::Align::START);

	m_hbox.append(m_frameFAILED);
	m_frameFAILED.set_hexpand(true);

	m_frameFAILED.set_child(m_vbox_failed);
	m_vbox_failed.append(m_failed_image);
	m_frameFAILED.set_label_align(0.5f);
	m_failed_image.set_margin_top(10);
	m_failed_image.set_margin_bottom(10);
	m_failed_image.set_margin_start(10);
	m_failed_image.set_margin_end(10);
	m_failed_image.set_halign(Gtk::Align::START);
	m_failed_image.set_valign(Gtk::Align::START);

	if (m_path.empty())
	{
		m_path = fs::path(__FILE__).parent_path().parent_path() / "render";
	}

	scan_tests();
}

void MainWindow::on_btnApprove_clicked()
{
	if(index_is_valid(m_current_test))
	{
		bool is_changed = m_tests[m_current_test].action != test_action_approve;
		m_tests[m_current_test].action = test_action_approve;
		if(is_changed) update_status();
	}
	show_next_test();
}

void MainWindow::on_btnBack_clicked()
{
	if(index_is_valid(m_current_test - 1))
	{
		load_render_test(m_tests[--m_current_test].html);
	}
}

void MainWindow::on_btnBrowser_clicked()
{
	if(index_is_valid(m_current_test))
	{
		try
		{
			Gio::AppInfo::launch_default_for_uri("file://" + m_tests[m_current_test].html);
		} catch (const Gio::Error&)
		{
			auto dialog = Gtk::AlertDialog::create("Error");
			dialog->set_detail("Failed to open the file in a browser");
			dialog->show(*this);
		}
	}
}

void MainWindow::on_btnDisable_clicked()
{
	if(index_is_valid(m_current_test))
	{
		bool is_changed = m_tests[m_current_test].action != test_action_disable;
		m_tests[m_current_test].action = test_action_disable;
		if(is_changed) update_status();
	}
	show_next_test();
}

void MainWindow::on_btnIgnore_clicked()
{
	if(index_is_valid(m_current_test))
	{
		bool is_changed = m_tests[m_current_test].action != test_action_ignore;
		m_tests[m_current_test].action = test_action_ignore;
		if(is_changed) update_status();
	}
	show_next_test();
}

void MainWindow::on_btnSave_clicked()
{
	for(const auto& test : m_tests)
	{
		if(test.action == test_action_approve)
		{
			auto path_from = fs::path(test.html + "-FAILED.png");
			auto path_to = fs::path(test.html + ".png");
			try
			{
				fs::rename(path_from, path_to);
			} catch (std::filesystem::filesystem_error const& ex)
			{
				auto dialog = Gtk::AlertDialog::create("Error");
				dialog->set_detail(ex.what());
				dialog->show(*this);
			}
		} else if(test.action == test_action_disable)
		{
			auto path_failed = fs::path(test.html + "-FAILED.png");
			auto path_from = fs::path(test.html);
			auto path_to = fs::path(test.html).parent_path() / fs::path("-" + fs::path(test.html).filename().string());
			try
			{
				fs::rename(path_from, path_to);
				fs::remove(path_failed);
			} catch (std::filesystem::filesystem_error const& ex)
			{
				auto dialog = Gtk::AlertDialog::create("Error");
				dialog->set_detail(ex.what());
				dialog->show(*this);
			}
		}
	}
	scan_tests();
}

void MainWindow::on_btnNext_clicked()
{
	show_next_test();
}

void MainWindow::load_render_test(const std::string &path)
{
	std::stringstream ss_FAILED;
	std::stringstream ss_OK;
	try
	{
		auto pixbuf_failed = Gdk::Pixbuf::create_from_file(path + "-FAILED.png");
		ss_FAILED << "[" << pixbuf_failed->get_width() << "x" << pixbuf_failed->get_height() << "]";
		m_failed_image.set_pixbuf(pixbuf_failed);
	} catch (...)
	{
		m_failed_image.set_pixbuf({});
	}
	try
	{
		auto pixbuf_ok = Gdk::Pixbuf::create_from_file(path + ".png");
		ss_OK << "[" << pixbuf_ok->get_width() << "x" << pixbuf_ok->get_height() << "]";
		m_image.set_pixbuf(pixbuf_ok);
	} catch (...)
	{
		m_image.set_pixbuf({});
	}
	m_labelFile.set_label("[ " + m_tests[m_current_test].status() + " ] " + path);
	m_frameOK.set_label(fs::path(path).filename().string() + ".png " + ss_OK.str());
	m_frameFAILED.set_label(fs::path(path).filename().string() + "-FAILED.png " + ss_FAILED.str());
}

void MainWindow::scan_tests()
{
	m_tests.clear();
	for(auto iter = fs::recursive_directory_iterator(m_path); iter != fs::recursive_directory_iterator(); ++iter)
	{
		if(iter->is_directory())
		{
			if(iter->path().filename().string().front() == '-')
			{
				iter.disable_recursion_pending();
			}
		} else
		{
			auto ext = iter->path().extension().string();
			if(iter->path().filename().string().front() != '-' && (ext == ".html" || ext == ".htm"))
			{
				std::string failed_image_path = iter->path().string() + "-FAILED.png";
				if(exists(fs::path(failed_image_path)))
				{
					m_tests.emplace_back(iter->path().string());
				}
			}
		}
	}
	std::sort(m_tests.begin(), m_tests.end(), [this](const auto &a, const auto &b) { return a.html < b.html; });
	m_current_test = -1;
	show_next_test();
	update_status();
}

void MainWindow::show_next_test()
{
	if(index_is_valid(m_current_test + 1))
	{
		load_render_test(m_tests[++m_current_test].html);
	}
}

void MainWindow::update_status()
{
	int num_ignored = 0;
	int num_approved = 0;
	int num_disabled = 0;
	for(const auto& test : m_tests)
	{
		switch (test.action)
		{
			case test_action_disable: num_disabled++; break;
			case test_action_approve: num_approved++; break;
			case test_action_ignore: num_ignored++; break;
			default: break;
		}
	}
	std::stringstream status;
	status << "I: " << num_ignored << "  A: " << num_approved << "  D: " << num_disabled;
	m_labelTitle.set_label(status.str());
}

void MainWindow::on_btnCfg_clicked()
{
	if(index_is_valid(m_current_test))
	{
		std::string html_path = m_tests[m_current_test].html;
		auto* dlg = new html_config_dialog(*this, html_path);
		dlg->set_transient_for(*this);
		dlg->signal_response().connect([this, dlg, html_path](int response)
		{
			if(response == Gtk::ResponseType::APPLY)
			{
				run_render2png(html_path);
			}
			delete dlg;
		});
		dlg->present();
	}
}

void MainWindow::run_render2png(const std::string& html_path)
{
	char self_path[PATH_MAX];
	ssize_t count = readlink("/proc/self/exe", self_path, PATH_MAX);
	if(count != -1)
	{
		self_path[count] = '\0';
		fs::path render2png_path = (fs::path(self_path).parent_path().parent_path() / "render2png");
		if(exists(render2png_path))
		{
			std::string cmd = "\"" + render2png_path.string() + "\" \"" + html_path +
					"\" \"" + html_path + "-FAILED.png\"";
			system(cmd.c_str());
			if(index_is_valid(m_current_test) && m_tests[m_current_test].html == html_path)
			{
				load_render_test(html_path);
			}
		}
	}
}

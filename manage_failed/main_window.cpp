#include "main_window.h"
#include <filesystem>
#include <sstream>
#include <iostream>
#include <unistd.h>
#include <linux/limits.h>
#include "config_dialog.h"

namespace fs = std::filesystem;

MainWindow::MainWindow(const std::string& path) : m_path(path), m_current_test(-1)
{
	set_titlebar(m_header);
	m_header.show();

	m_header.set_show_close_button(true);
	m_header.property_spacing().set_value(10);

	m_header.pack_start(m_btnCfg);
	m_btnCfg.show();
	m_btnCfg.signal_clicked().connect( sigc::mem_fun(*this, &MainWindow::on_btnCfg_clicked) );
	m_btnCfg.set_image_position(Gtk::POS_TOP);
	m_btnCfg.set_label("Config");
	m_btnCfg.set_image_from_icon_name("accessories-text-editor-symbolic", Gtk::ICON_SIZE_BUTTON);

	m_header.pack_start(m_btnBack);
	m_btnBack.show();
	m_btnBack.signal_clicked().connect( sigc::mem_fun(*this, &MainWindow::on_btnBack_clicked) );
	m_btnBack.set_image_position(Gtk::POS_TOP);
	m_btnBack.set_label("Back");
	m_btnBack.set_image_from_icon_name("media-skip-backward-symbolic", Gtk::ICON_SIZE_BUTTON);

	m_header.pack_start(m_btnNext);
	m_btnNext.show();
	m_btnNext.signal_clicked().connect( sigc::mem_fun(*this, &MainWindow::on_btnNext_clicked) );
	m_btnNext.set_image_position(Gtk::POS_TOP);
	m_btnNext.set_label("Next");
	m_btnNext.set_image_from_icon_name("media-skip-forward-symbolic", Gtk::ICON_SIZE_BUTTON);

	m_header.pack_start(m_sep);
	m_sep.show();

	m_header.pack_start(m_btnApprove);
	m_btnApprove.show();
	m_btnApprove.signal_clicked().connect( sigc::mem_fun(*this, &MainWindow::on_btnApprove_clicked) );
	m_btnApprove.set_image_position(Gtk::POS_TOP);
	m_btnApprove.set_label("Approve");
	m_btnApprove.set_image_from_icon_name("emblem-default-symbolic", Gtk::ICON_SIZE_BUTTON);

	m_header.pack_start(m_btnIgnore);
	m_btnIgnore.show();
	m_btnIgnore.signal_clicked().connect( sigc::mem_fun(*this, &MainWindow::on_btnIgnore_clicked) );
	m_btnIgnore.set_image_position(Gtk::POS_TOP);
	m_btnIgnore.set_label("Ignore");
	m_btnIgnore.set_image_from_icon_name("media-seek-forward-symbolic", Gtk::ICON_SIZE_BUTTON);

	m_header.pack_start(m_btnDisable);
	m_btnDisable.show();
	m_btnDisable.signal_clicked().connect( sigc::mem_fun(*this, &MainWindow::on_btnDisable_clicked) );
	m_btnDisable.set_image_position(Gtk::POS_TOP);
	m_btnDisable.set_label("Disable");
	m_btnDisable.set_image_from_icon_name("off-symbolic", Gtk::ICON_SIZE_BUTTON);

	m_header.pack_end(m_btnSave);
	m_btnSave.show();
	m_btnSave.signal_clicked().connect( sigc::mem_fun(*this, &MainWindow::on_btnSave_clicked) );
	m_btnSave.set_image_position(Gtk::POS_TOP);
	m_btnSave.set_label("Save");
	m_btnSave.set_image_from_icon_name("document-save-symbolic", Gtk::ICON_SIZE_BUTTON);

	m_header.pack_end(m_btnBrowser);
	m_btnBrowser.show();
	m_btnBrowser.signal_clicked().connect( sigc::mem_fun(*this, &MainWindow::on_btnBrowser_clicked) );
	m_btnBrowser.set_image_position(Gtk::POS_TOP);
	m_btnBrowser.set_label("Browser");
	m_btnBrowser.set_image_from_icon_name("network-workgroup-symbolic", Gtk::ICON_SIZE_BUTTON);

	m_header.set_custom_title(m_labelTitle);
	m_labelTitle.show();
	m_labelTitle.set_hexpand_set(true);
	m_labelTitle.set_hexpand();
	m_labelTitle.show();

	add(m_vbox);
	m_vbox.show();

	m_vbox.pack_start(m_labelFile, Gtk::PACK_SHRINK, 0);
	m_labelFile.show();

	m_vbox.pack_start(m_hbox, Gtk::PACK_EXPAND_WIDGET, 10);
	m_hbox.show();

	m_hbox.pack_start(m_frameOK, Gtk::PACK_EXPAND_WIDGET, 10);
	m_frameOK.set_label_align(0.5);
	m_frameOK.show();

	m_frameOK.add(m_vbox_ok);
	m_vbox_ok.show();
	m_vbox_ok.pack_start(m_image, Gtk::PACK_EXPAND_WIDGET, 0);
	m_image.set_margin_top(10);
	m_image.set_margin_bottom(10);
	m_image.set_margin_left(10);
	m_image.set_margin_right(10);
	m_image.set_alignment(1, 0);
	m_image.show();

	m_hbox.pack_start(m_frameFAILED, Gtk::PACK_EXPAND_WIDGET, 10);
	m_frameFAILED.show();

	m_frameFAILED.add(m_vbox_failed);
	m_vbox_failed.show();
	m_vbox_failed.pack_start(m_failed_image, Gtk::PACK_EXPAND_WIDGET, 0);
	m_frameFAILED.set_label_align(0.5);
	m_failed_image.set_margin_top(10);
	m_failed_image.set_margin_bottom(10);
	m_failed_image.set_margin_left(10);
	m_failed_image.set_margin_right(10);
	m_failed_image.set_alignment(0, 0);
	m_failed_image.show();

	m_path = fs::path(__FILE__).parent_path().parent_path() / "render";
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
		gtk_show_uri_on_window(nullptr, ("file://" + m_tests[m_current_test].html).c_str(), 0, nullptr);
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
				Gtk::MessageDialog dialog(ex.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
				dialog.run();
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
				Gtk::MessageDialog dialog(ex.what(), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
				dialog.run();
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
		m_failed_image.set(pixbuf_failed);
	} catch (...)
	{
		m_failed_image.set_from_icon_name("image-missing-symbolic", Gtk::ICON_SIZE_DIALOG);
	}
	try
	{
		auto pixbuf_ok = Gdk::Pixbuf::create_from_file(path + ".png");
		ss_OK << "[" << pixbuf_ok->get_width() << "x" << pixbuf_ok->get_height() << "]";
		m_image.set(pixbuf_ok);
	} catch (...)
	{
		m_image.set_from_icon_name("image-missing-symbolic", Gtk::ICON_SIZE_DIALOG);
	}
	m_labelFile.set_label("[ " + m_tests[m_current_test].status() + " ] " + path);
	m_frameOK.set_label(fs::path(path).filename().string() + ".png " + ss_OK.str());
	m_frameFAILED.set_label(fs::path(path).filename().string() + "-FAILED.png " + ss_FAILED.str());
	resize(1, 1);
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
		html_config_dialog dlg(*this, m_tests[m_current_test].html);
		if(dlg.run() == Gtk::RESPONSE_APPLY)
		{
			char self_path[PATH_MAX];
			ssize_t count = readlink("/proc/self/exe", self_path, PATH_MAX);
			if(count != -1)
			{
				fs::path render2png_path = (fs::path(self_path).parent_path().parent_path() / "render2png");
				if(exists(render2png_path))
				{
					std::string cmd = "\"" + render2png_path.string() + "\" \"" + m_tests[m_current_test].html +
							"\" \"" + m_tests[m_current_test].html + "-FAILED.png\"";
					system(cmd.c_str());
					load_render_test(m_tests[m_current_test].html);
				}
			}
		}
	}
}

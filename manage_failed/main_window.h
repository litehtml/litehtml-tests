#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <gtkmm/window.h>
#include <string>
#include <utility>
#include <gtkmm.h>

enum test_action
{
	test_action_ignore,
	test_action_approve,
	test_action_disable,
};

struct test_entry
{
	std::string html;
	test_action action;

	explicit test_entry(std::string  _html) : html(std::move(_html)), action(test_action_ignore) {}
	std::string status() const
	{
		switch (action)
		{
			case test_action_ignore: return "ignore";
			case test_action_approve: return "approve";
			case test_action_disable: return "disable";
		}
		return "unknown";
	}
};

class MainWindow : public Gtk::Window
{
	std::string		m_path;
	Gtk::HeaderBar	m_header;
	Gtk::Button		m_btnApprove;
	Gtk::Button		m_btnBrowser;
	Gtk::Button		m_btnDisable;
	Gtk::Button		m_btnIgnore;
	Gtk::Button		m_btnBack;
	Gtk::Button		m_btnSave;
	Gtk::Button		m_btnNext;
	Gtk::Button		m_btnCfg;
	Gtk::Separator	m_sep;
	Gtk::HBox		m_hbox;
	Gtk::VBox		m_vbox;
	Gtk::Image		m_image;
	Gtk::Image		m_failed_image;
	Gtk::Label		m_labelFile;
	Gtk::VBox		m_vbox_ok;
	Gtk::VBox		m_vbox_failed;
	Gtk::Frame		m_frameOK;
	Gtk::Frame		m_frameFAILED;
	Gtk::Label		m_labelTitle;

	std::vector<test_entry> m_tests;
	int m_current_test;
public:
	explicit MainWindow(const std::string& path);

private:
	void on_btnApprove_clicked();
	void on_btnBack_clicked();
	void on_btnBrowser_clicked();
	void on_btnDisable_clicked();
	void on_btnIgnore_clicked();
	void on_btnSave_clicked();
	void on_btnNext_clicked();
	void load_render_test(const std::string& path);
	void scan_tests();
	void show_next_test();
	bool index_is_valid(int index) const { return index >= 0 && index < (int) m_tests.size(); }
	void update_status();
	void on_btnCfg_clicked();
};

#endif //MAIN_WINDOW_H

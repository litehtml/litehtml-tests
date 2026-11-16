#ifndef MANAGE_TESTS_SEARCH_DIALOG_H
#define MANAGE_TESTS_SEARCH_DIALOG_H

#include <gtkmm.h>

#include <string>
#include <vector>

#include "test_model.h"

// Modal "find test" dialog: a search entry plus a live-updating list of
// matching tests. Emits signal_test_selected(index) with the index into the
// test list when a result is selected.
class search_dialog : public Gtk::Dialog
{
public:
    search_dialog(Gtk::Window& parent, const std::vector<test_entry>& tests,
                  const std::string& tests_folder, int current_index);

    sigc::signal<void(int)>& signal_test_selected() { return m_signal_selected; }

private:
    const std::vector<test_entry>& m_tests;
    std::string m_tests_folder;
    std::vector<std::string> m_display_paths;
    int m_current_index;

    Gtk::SearchEntry m_search;
    Gtk::ScrolledWindow m_scroll;
    Gtk::ListBox m_list;
    bool m_updating = false;

    sigc::signal<void(int)> m_signal_selected;

    void on_search_changed();
    void on_row_selected(Gtk::ListBoxRow* row);
};

#endif // MANAGE_TESTS_SEARCH_DIALOG_H

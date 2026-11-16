#include "search_dialog.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace
{
    std::string to_lower(const std::string& s)
    {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return r;
    }

    class test_row : public Gtk::ListBoxRow
    {
    public:
        test_row(const std::string& text, int index) : m_index(index)
        {
            auto* label = Gtk::make_managed<Gtk::Label>(text);
            label->set_xalign(0.0f);
            label->set_ellipsize(Pango::EllipsizeMode::MIDDLE);
            set_child(*label);
        }

        int index() const { return m_index; }

    private:
        int m_index;
    };
}

search_dialog::search_dialog(Gtk::Window& parent, const std::vector<test_entry>& tests,
                             const std::string& tests_folder, int current_index)
    : Gtk::Dialog("Search Tests", parent, true, true),
      m_tests(tests),
      m_tests_folder(tests_folder),
      m_current_index(current_index)
{
    // Precompute the display path for every test once (purely lexical, no
    // filesystem access), so the per-keystroke loop only does string matching.
    m_display_paths.reserve(m_tests.size());
    for (const auto& t : m_tests)
    {
        auto rel = fs::path(t.html).lexically_relative(m_tests_folder);
        m_display_paths.push_back(rel.empty() ? fs::path(t.html).filename().string() : rel.string());
    }

    add_button("Close", Gtk::ResponseType::CLOSE);

    m_search.set_placeholder_text("Search tests…");
    m_search.set_hexpand(true);
    m_search.set_margin(8);
    m_search.signal_changed().connect(sigc::mem_fun(*this, &search_dialog::on_search_changed));

    m_list.set_selection_mode(Gtk::SelectionMode::SINGLE);
    m_list.set_hexpand(true);
    m_list.set_vexpand(true);
    m_list.signal_row_selected().connect(sigc::mem_fun(*this, &search_dialog::on_row_selected));

    m_scroll.set_child(m_list);
    m_scroll.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_scroll.set_hexpand(true);
    m_scroll.set_vexpand(true);

    auto* content = get_content_area();
    content->append(m_search);
    content->append(m_scroll);

    on_search_changed(); // initial population

    set_default_size(500, 600);
}

void search_dialog::on_search_changed()
{
    const std::string query = to_lower(m_search.get_text());

    m_updating = true;
    m_list.remove_all();

    Gtk::ListBoxRow* current_row = nullptr;
    for (size_t i = 0; i < m_tests.size(); ++i)
    {
        const std::string& path = m_display_paths[i];
        if (!query.empty() && to_lower(path).find(query) == std::string::npos)
            continue;

        auto* row = Gtk::make_managed<test_row>(path, static_cast<int>(i));
        m_list.append(*row);
        if (static_cast<int>(i) == m_current_index)
            current_row = row;
    }

    if (current_row)
        m_list.select_row(*current_row);

    m_updating = false;
}

void search_dialog::on_row_selected(Gtk::ListBoxRow* row)
{
    if (m_updating || !row)
        return;
    auto* tr = dynamic_cast<test_row*>(row);
    if (!tr)
        return;
    m_signal_selected.emit(tr->index());
    response(Gtk::ResponseType::CLOSE);
}

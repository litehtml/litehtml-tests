#include "config_dialog.h"

#include <filesystem>
#include <fstream>

namespace
{
    std::string trim(const std::string& s)
    {
        const auto first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return {};
        const auto last = s.find_last_not_of(" \t\r\n");
        return s.substr(first, last - first + 1);
    }

    // Split a config line on colons outside double-quoted segments, trim each
    // token and drop empty ones. Mirrors how render2png parses `.cfg` files.
    std::vector<std::string> split_colon(const std::string& line)
    {
        std::vector<std::string> tokens;
        std::string cur;
        bool in_quotes = false;
        for (char c : line)
        {
            if (c == '"')
                in_quotes = !in_quotes;
            if (c == ':' && !in_quotes)
            {
                std::string t = trim(cur);
                if (!t.empty())
                    tokens.push_back(std::move(t));
                cur.clear();
            }
            else
            {
                cur += c;
            }
        }
        std::string t = trim(cur);
        if (!t.empty())
            tokens.push_back(std::move(t));
        return tokens;
    }
}

config_dialog::int_field::int_field(const std::string& k)
    : key(k),
      set(k),
      spin(Gtk::Adjustment::create(0.0, -1000000.0, 1000000.0, 1.0, 10.0, 0.0))
{
    spin.set_digits(0);
    spin.set_numeric(true);
    spin.set_sensitive(false);
    set.set_tooltip_text("Write '" + key + "' to the config file");
    spin.set_tooltip_text("Value for '" + key + "'");
}

config_dialog::config_dialog(Gtk::Window& parent, const std::string& html_path)
    : Gtk::Dialog("Test Config", parent, true, true), m_cfg_file(html_path + ".cfg")
{
    add_button("Cancel", Gtk::ResponseType::CANCEL);
    get_header_bar()->pack_end(m_btnSave);
    m_btnSave.signal_clicked().connect(sigc::mem_fun(*this, &config_dialog::on_btnSave_clicked));

    m_int_fields.push_back(std::make_unique<int_field>("width"));
    m_int_fields.push_back(std::make_unique<int_field>("height"));
    m_int_fields.push_back(std::make_unique<int_field>("drawx"));
    m_int_fields.push_back(std::make_unique<int_field>("drawy"));
    m_int_fields.push_back(std::make_unique<int_field>("wheelx"));
    m_int_fields.push_back(std::make_unique<int_field>("wheely"));
    m_int_fields.push_back(std::make_unique<int_field>("wheelatx"));
    m_int_fields.push_back(std::make_unique<int_field>("wheelaty"));

    m_grid.set_row_spacing(6);
    m_grid.set_column_spacing(12);
    m_grid.set_margin(12);

    m_bestfit.set_tooltip_text("Enable best-fit width layout (bestfit)");
    m_bestfit.set_active(true); // render2png default
    m_grid.attach(m_bestfit, 0, 0, 2, 1);

    int row = 1;
    for (auto& f : m_int_fields)
    {
        int_field* p = f.get();
        f->set.signal_toggled().connect([p] { p->spin.set_sensitive(p->set.get_active()); });
        m_grid.attach(f->set, 0, row, 1, 1);
        f->spin.set_hexpand(true);
        f->spin.set_halign(Gtk::Align::FILL);
        m_grid.attach(f->spin, 1, row, 1, 1);
        row++;
    }

    m_other_label.set_text("Other options (one \"key: value\" per line):");
    m_other_label.set_xalign(0.0f);
    m_other_label.set_margin_top(12);
    m_other_label.set_margin_start(12);
    m_other_label.set_margin_end(12);

    m_scroll.set_hexpand(true);
    m_scroll.set_vexpand(true);
    m_scroll.set_child(m_other);
    m_other.set_editable(true);
    m_other.set_monospace(true);

    auto* content = get_content_area();
    content->set_margin_bottom(12);
    content->set_margin_start(12);
    content->set_margin_end(12);
    content->append(m_grid);
    content->append(m_other_label);
    content->append(m_scroll);

    load_config();

    set_default_size(480, 520);
}

void config_dialog::load_config()
{
    std::string other;
    std::ifstream infile(m_cfg_file);
    if (infile.is_open())
    {
        for (std::string line; std::getline(infile, line);)
        {
            auto tokens = split_colon(line);
            bool handled = false;

            if (tokens.size() == 2)
            {
                const std::string& key = tokens[0];
                const std::string& value = tokens[1];

                if (key == "bestfit")
                {
                    if (value == "true")
                    {
                        m_bestfit.set_active(true);
                        handled = true;
                    }
                    else if (value == "false")
                    {
                        m_bestfit.set_active(false);
                        handled = true;
                    }
                }
                else
                {
                    for (auto& f : m_int_fields)
                    {
                        if (f->key == key)
                        {
                            try
                            {
                                f->spin.set_value(std::stoi(value));
                                f->set.set_active(true);
                                handled = true;
                            }
                            catch (...)
                            {
                                // Unparseable value: leave the line in "Other".
                            }
                            break;
                        }
                    }
                }
            }

            if (!handled)
            {
                if (!other.empty())
                    other += '\n';
                other += line;
            }
        }
    }
    m_other.get_buffer()->set_text(other);
}

bool config_dialog::save_config()
{
    std::string out;
    // bestfit defaults to true, so only write it when it is disabled.
    if (!m_bestfit.get_active())
        out += "bestfit: false\n";
    for (const auto& f : m_int_fields)
    {
        if (f->set.get_active())
            out += f->key + ": " + std::to_string(f->spin.get_value_as_int()) + "\n";
    }

    std::string other = m_other.get_buffer()->get_text();
    if (!other.empty() && other.back() != '\n')
        other += '\n';
    out += other;

    // Nothing meaningful to store: remove the sidecar file if it exists.
    if (trim(out).empty())
    {
        std::error_code ec;
        std::filesystem::remove(m_cfg_file, ec);
        if (ec)
        {
            auto dlg = Gtk::AlertDialog::create("ERROR: Impossible to save file");
            dlg->show(*this);
            return false;
        }
        return true;
    }

    bool saved = false;
    try
    {
        std::ofstream outfile(m_cfg_file, std::ios::out | std::ios::trunc);
        if (outfile.is_open())
        {
            outfile << out;
            saved = true;
        }
    }
    catch (...)
    {
    }

    if (!saved)
    {
        auto dlg = Gtk::AlertDialog::create("ERROR: Impossible to save file");
        dlg->show(*this);
    }
    return saved;
}

void config_dialog::on_btnSave_clicked()
{
    if (save_config())
        response(Gtk::ResponseType::APPLY);
}

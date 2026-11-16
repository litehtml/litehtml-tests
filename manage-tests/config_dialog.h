#ifndef MANAGE_TESTS_CONFIG_DIALOG_H
#define MANAGE_TESTS_CONFIG_DIALOG_H

#include <gtkmm.h>

#include <memory>
#include <string>
#include <vector>

// Editor for a render2png `.cfg` sidecar file (`<html>.cfg`). The fields that
// render2png understands are exposed as GTK controls; anything else is shown
// verbatim in a free-form text area and preserved on save.
class config_dialog : public Gtk::Dialog
{
public:
    config_dialog(Gtk::Window& parent, const std::string& html_path);

private:
    // One integer-valued config key ("width", "height", "drawx", ...). The
    // checkbox controls whether the key is written to the file (unchecked =
    // omit, so render2png falls back to its default); the spin holds the value.
    struct int_field
    {
        std::string key;
        Gtk::CheckButton set;
        Gtk::SpinButton spin;

        explicit int_field(const std::string& k);
    };

    std::string m_cfg_file;

    Gtk::CheckButton m_bestfit{"bestfit"};
    std::vector<std::unique_ptr<int_field>> m_int_fields;

    Gtk::Grid m_grid;
    Gtk::Label m_other_label;
    Gtk::ScrolledWindow m_scroll;
    Gtk::TextView m_other;
    Gtk::Button m_btnSave{"Save"};

    void load_config();
    bool save_config();
    void on_btnSave_clicked();
};

#endif // MANAGE_TESTS_CONFIG_DIALOG_H

#ifndef MANAGE_TESTS_MAIN_WINDOW_H
#define MANAGE_TESTS_MAIN_WINDOW_H

#include <gtkmm.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "chrome_render.h"
#include "docker_render.h"
#include "test_model.h"

class MainWindow : public Gtk::Window
{
public:
    MainWindow(const std::string& tests_folder, const std::string& litehtml_folder);
    ~MainWindow() override;

private:
    enum class left_source { golden, render2png, chrome };
    enum class right_source { failed, chrome };

    // configuration
    std::string m_tests_folder;
    std::string m_litehtml_folder;
    std::string m_repo;
    std::string m_cache_dir;

    // state
    test_mode m_mode = test_mode::failed;
    left_source m_left = left_source::golden;
    right_source m_right = right_source::failed;
    std::vector<test_entry> m_tests;
    int m_current = -1;
    int m_generation = 0;

    // render errors keyed by output cache path, for surfacing in the UI
    std::map<std::string, std::string> m_render_errors;

    // synchronized scrolling state
    bool m_ctrl_down = false;
    bool m_syncing = false;

    // renderers
    docker_render m_docker;
    chrome_render m_chrome;
    std::string m_docker_error;
    std::string m_chrome_error;

    // async dispatch to the main thread
    Glib::Dispatcher m_dispatcher;
    std::mutex m_queue_mutex;
    std::deque<std::function<void()>> m_queue;

    // worker thread (serializes docker render2png + chrome)
    struct render_job
    {
        std::string html;       // absolute html path
        std::string r2png_out;  // absolute cache path for render2png output
        std::string chrome_out; // absolute cache path for chrome output
        int gen = -1;
        bool need_render2png = false;
        bool need_chrome = false;
        bool valid = false;
    };
    std::mutex m_job_mutex;
    std::condition_variable m_job_cv;
    render_job m_job;
    bool m_worker_stop = false;
    std::atomic<bool> m_need_init{true};
    std::mutex m_init_mutex;
    std::string m_init_tests_folder;
    std::thread m_worker;

    // widgets
    Gtk::HeaderBar m_header;
    Gtk::ComboBoxText m_comboMode;
    Gtk::Button m_btnFolder;
    Gtk::Button m_btnCopy;
    Gtk::Button m_btnPrev;
    Gtk::Button m_btnNext;
    Gtk::Button m_btnApprove;
    Gtk::Button m_btnDisable;
    Gtk::Button m_btnIgnore;
    Gtk::Button m_btnSave;
    Gtk::Separator m_sep{Gtk::Orientation::VERTICAL};
    Gtk::Label m_labelTitle;
    Gtk::Label m_labelFile;
    Gtk::Box m_rootBox{Gtk::Orientation::VERTICAL, 0};
    Gtk::Paned m_paned{Gtk::Orientation::HORIZONTAL};
    Gtk::Frame m_frameLeft;
    Gtk::Frame m_frameRight;
    Gtk::Box m_vboxLeft{Gtk::Orientation::VERTICAL, 0};
    Gtk::Box m_vboxRight{Gtk::Orientation::VERTICAL, 0};
    Gtk::Box m_leftButtons{Gtk::Orientation::HORIZONTAL, 4};
    Gtk::ToggleButton m_btnLeftGolden;
    Gtk::ToggleButton m_btnLeftSecond;
    Gtk::Box m_rightButtons{Gtk::Orientation::HORIZONTAL, 4};
    Gtk::ToggleButton m_btnRightFailed;
    Gtk::ToggleButton m_btnRightChrome;
    Gtk::ScrolledWindow m_scrollLeft;
    Gtk::ScrolledWindow m_scrollRight;
    Gtk::Picture m_imageLeft;
    Gtk::Picture m_imageRight;

    bool m_in_update = false;

    bool index_is_valid(int i) const { return i >= 0 && i < static_cast<int>(m_tests.size()); }
    std::string render2png_cache_path(const test_entry& t) const;
    std::string chrome_cache_path(const test_entry& t) const;
    std::string approved_cache_path(const test_entry& t) const;
    static std::string rel_of(const std::string& path, const std::string& base);

    void post_to_main(std::function<void()> fn);
    void drain_queue();

    void setup_ui();
    void setup_shortcuts();
    void setup_scroll_sync();
    void link_adjustments(const Glib::RefPtr<Gtk::Adjustment>& src, const Glib::RefPtr<Gtk::Adjustment>& dst);
    bool on_key_pressed(guint keyval, guint keycode, Gdk::ModifierType state);
    void on_key_released(guint keyval, guint keycode, Gdk::ModifierType state);
    void update_left_button_label();
    void worker_loop();

    void scan_tests();
    void load_current_test();
    void show_next_test();
    void invalidate_async();
    void refresh_images();
    void render_left_image(const test_entry& t);
    void render_right_image(const test_entry& t);
    void set_image(Gtk::Picture& img, const std::string& path);
    std::string frame_label(const std::string& label, const std::string& path) const;
    void request_renders();
    void update_buttons();
    bool approve_enabled() const;
    bool disable_enabled() const;
    bool has_pending_changes() const;
    void update_title();
    void show_error(const std::string& msg);
    void restart_renderers();

    // handlers
    void on_mode_changed();
    void on_left_source_changed();
    void on_right_source_changed();
    void on_btnFolder_clicked();
    void copy_test_path();
    void on_btnPrev_clicked();
    void on_btnNext_clicked();
    void on_btnApprove_clicked();
    void on_btnDisable_clicked();
    void on_btnIgnore_clicked();
    void on_btnSave_clicked();
};

#endif // MANAGE_TESTS_MAIN_WINDOW_H

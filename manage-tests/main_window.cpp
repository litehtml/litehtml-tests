#include "main_window.h"

#include "util.h"

#include <gdk/gdkkeysyms.h>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <unistd.h>

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

    std::string filename_of(const std::string& path)
    {
        return fs::path(path).filename().string();
    }

    std::string dims_of(const std::string& path)
    {
        int w = 0, h = 0;
        if (read_png_size(path, w, h))
            return "[" + std::to_string(w) + "x" + std::to_string(h) + "]";
        return "";
    }

    std::string first_line(const std::string& s, size_t max_len = 80)
    {
        size_t end = s.find('\n');
        std::string line = (end == std::string::npos) ? s : s.substr(0, end);
        if (line.size() > max_len)
            line = line.substr(0, max_len) + "...";
        return line;
    }
}

MainWindow::MainWindow(const std::string& tests_folder, const std::string& litehtml_folder)
    : m_tests_folder(tests_folder), m_litehtml_folder(litehtml_folder), m_current(-1)
{
    m_repo = fs::absolute(MANAGE_TESTS_REPO).lexically_normal().string();
    m_tests_folder = fs::absolute(m_tests_folder).lexically_normal().string();
    m_litehtml_folder = fs::absolute(m_litehtml_folder).lexically_normal().string();
    m_cache_dir = (fs::temp_directory_path() / "litehtml-manage-tests").string();
    fs::create_directories(m_cache_dir);

    // Render outputs are always regenerated on startup.
    std::error_code ec;
    fs::remove_all(fs::path(m_cache_dir) / "chrome", ec);
    fs::remove_all(fs::path(m_cache_dir) / "render2png", ec);

    m_docker_error = "building";
    m_chrome_error = "building";

    setup_ui();
    setup_shortcuts();

    // Set the initial toggle button states to match the default sources.
    update_left_button_label();
    m_in_update = true;
    m_btnLeftGolden.set_active(m_left == left_source::golden);
    m_btnLeftSecond.set_active(m_left != left_source::golden);
    m_btnRightFailed.set_active(m_right == right_source::failed);
    m_btnRightChrome.set_active(m_right == right_source::chrome);
    m_in_update = false;

    m_dispatcher.connect([this] { drain_queue(); });

    {
        std::lock_guard<std::mutex> lk(m_init_mutex);
        m_init_tests_folder = m_tests_folder;
    }

    m_worker = std::thread([this] { worker_loop(); });

    scan_tests();

    set_default_size(1100, 700);
}

MainWindow::~MainWindow()
{
    {
        std::lock_guard<std::mutex> lk(m_job_mutex);
        m_worker_stop = true;
    }
    m_job_cv.notify_all();
    // Kill the container so an in-flight build/exec aborts and join() returns quickly.
    run_command("docker rm -f " + shell_quote(docker_render::make_container_name()));
    if (m_worker.joinable())
        m_worker.join();
    m_docker.shutdown();
}

void MainWindow::post_to_main(std::function<void()> fn)
{
    {
        std::lock_guard<std::mutex> lk(m_queue_mutex);
        m_queue.push_back(std::move(fn));
    }
    m_dispatcher.emit();
}

void MainWindow::drain_queue()
{
    std::function<void()> fn;
    while (true)
    {
        {
            std::lock_guard<std::mutex> lk(m_queue_mutex);
            if (m_queue.empty())
                return;
            fn = std::move(m_queue.front());
            m_queue.pop_front();
        }
        fn();
    }
}

void MainWindow::setup_ui()
{
    set_titlebar(m_header);
    m_header.set_show_title_buttons(true);

    // Mode switcher
    m_comboMode.append("Manage Failed");
    m_comboMode.append("Manage Passed");
    m_comboMode.append("Manage Disabled");
    m_comboMode.set_active(0);
    m_comboMode.set_valign(Gtk::Align::CENTER);
    m_comboMode.signal_changed().connect([this] { on_mode_changed(); });
    m_header.pack_start(m_comboMode);

    m_header.pack_start(m_btnFolder);
    m_btnFolder.signal_clicked().connect([this] { on_btnFolder_clicked(); });
    set_tool_button(m_btnFolder, "folder-open-symbolic", "Folder");
    m_btnFolder.set_tooltip_text("Select tests folder");

    m_header.pack_start(m_btnCopy);
    m_btnCopy.signal_clicked().connect([this] { copy_test_path(); });
    set_tool_button(m_btnCopy, "edit-copy-symbolic", "Copy");
    m_btnCopy.set_tooltip_text("Copy test path (Ctrl+C)");

    m_header.pack_start(m_btnPrev);
    m_btnPrev.signal_clicked().connect([this] { on_btnPrev_clicked(); });
    set_tool_button(m_btnPrev, "media-skip-backward-symbolic", "Prev");
    m_btnPrev.set_tooltip_text("Previous test (Ctrl+Left)");

    m_header.pack_start(m_btnNext);
    m_btnNext.signal_clicked().connect([this] { on_btnNext_clicked(); });
    set_tool_button(m_btnNext, "media-skip-forward-symbolic", "Next");
    m_btnNext.set_tooltip_text("Next test (Ctrl+Right)");

    m_header.pack_start(m_sep);

    m_header.pack_start(m_btnApprove);
    m_btnApprove.signal_clicked().connect([this] { on_btnApprove_clicked(); });
    set_tool_button(m_btnApprove, "emblem-default-symbolic", "Approve");
    m_btnApprove.set_tooltip_text("Approve (Ctrl+A)");

    m_header.pack_start(m_btnDisable);
    m_btnDisable.signal_clicked().connect([this] { on_btnDisable_clicked(); });
    set_tool_button(m_btnDisable, "off-symbolic", "Disable");
    m_btnDisable.set_tooltip_text("Disable (Ctrl+D)");

    m_header.pack_start(m_btnIgnore);
    m_btnIgnore.signal_clicked().connect([this] { on_btnIgnore_clicked(); });
    set_tool_button(m_btnIgnore, "media-seek-forward-symbolic", "Ignore");
    m_btnIgnore.set_tooltip_text("Ignore / remove marks (Ctrl+Space)");

    m_header.pack_end(m_btnSave);
    m_btnSave.signal_clicked().connect([this] { on_btnSave_clicked(); });
    set_tool_button(m_btnSave, "document-save-symbolic", "Save");
    m_btnSave.set_tooltip_text("Apply all marks (Ctrl+S)");

    m_header.set_title_widget(m_labelTitle);
    m_labelTitle.set_hexpand(true);

    // Body
    set_child(m_rootBox);
    m_rootBox.append(m_labelFile);
    m_labelFile.set_xalign(0.0f);
    m_labelFile.set_margin_start(8);
    m_labelFile.set_margin_top(4);
    m_labelFile.set_margin_bottom(4);
    m_labelFile.set_ellipsize(Pango::EllipsizeMode::MIDDLE);

    m_rootBox.append(m_paned);
    m_paned.set_vexpand(true);

    // Left pane
    m_paned.set_start_child(m_frameLeft);
    m_frameLeft.set_child(m_vboxLeft);
    m_frameLeft.set_label("Golden");
    m_vboxLeft.append(m_leftButtons);
    m_leftButtons.set_halign(Gtk::Align::START);
    m_leftButtons.set_margin_start(4);
    m_leftButtons.set_margin_top(4);
    m_leftButtons.append(m_btnLeftGolden);
    m_btnLeftGolden.set_label("Golden");
    m_btnLeftGolden.set_tooltip_text("Show the golden image");
    m_btnLeftGolden.signal_toggled().connect([this] { on_left_source_changed(); });
    m_leftButtons.append(m_btnLeftSecond);
    m_btnLeftSecond.set_tooltip_text("Show the alternative image");
    m_btnLeftSecond.set_group(m_btnLeftGolden);
    m_btnLeftSecond.signal_toggled().connect([this] { on_left_source_changed(); });
    m_vboxLeft.append(m_scrollLeft);
    m_scrollLeft.set_vexpand(true);
    m_scrollLeft.set_hexpand(true);
    m_scrollLeft.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_scrollLeft.set_child(m_imageLeft);
    m_imageLeft.set_can_shrink(false);
    m_imageLeft.set_content_fit(Gtk::ContentFit::SCALE_DOWN);
    m_imageLeft.set_halign(Gtk::Align::END);
    m_imageLeft.set_valign(Gtk::Align::START);
    m_imageLeft.set_margin_top(4);
    m_imageLeft.set_margin_bottom(4);
    m_imageLeft.set_margin_start(4);
    m_imageLeft.set_margin_end(4);

    // Right pane
    m_paned.set_end_child(m_frameRight);
    m_frameRight.set_child(m_vboxRight);
    m_frameRight.set_label("FAILED");
    m_vboxRight.append(m_rightButtons);
    m_rightButtons.set_halign(Gtk::Align::START);
    m_rightButtons.set_margin_start(4);
    m_rightButtons.set_margin_top(4);
    m_rightButtons.append(m_btnRightFailed);
    m_btnRightFailed.set_label("FAILED");
    m_btnRightFailed.set_tooltip_text("Show the -FAILED.png image");
    m_btnRightFailed.signal_toggled().connect([this] { on_right_source_changed(); });
    m_rightButtons.append(m_btnRightChrome);
    m_btnRightChrome.set_label("Chrome");
    m_btnRightChrome.set_tooltip_text("Show the Google Chrome render");
    m_btnRightChrome.set_group(m_btnRightFailed);
    m_btnRightChrome.signal_toggled().connect([this] { on_right_source_changed(); });
    m_vboxRight.append(m_scrollRight);
    m_scrollRight.set_vexpand(true);
    m_scrollRight.set_hexpand(true);
    m_scrollRight.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_scrollRight.set_child(m_imageRight);
    m_imageRight.set_can_shrink(false);
    m_imageRight.set_content_fit(Gtk::ContentFit::SCALE_DOWN);
    m_imageRight.set_halign(Gtk::Align::START);
    m_imageRight.set_valign(Gtk::Align::START);
    m_imageRight.set_margin_top(4);
    m_imageRight.set_margin_bottom(4);
    m_imageRight.set_margin_start(4);
    m_imageRight.set_margin_end(4);

    m_paned.set_position(550);

    setup_scroll_sync();
}

void MainWindow::setup_shortcuts()
{
    auto key = Gtk::EventControllerKey::create();
    key->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    key->signal_key_pressed().connect(sigc::mem_fun(*this, &MainWindow::on_key_pressed), false);
    key->signal_key_released().connect(sigc::mem_fun(*this, &MainWindow::on_key_released));
    add_controller(key);
}

bool MainWindow::on_key_pressed(guint keyval, guint /*keycode*/, Gdk::ModifierType state)
{
    if (keyval == GDK_KEY_Control_L || keyval == GDK_KEY_Control_R)
    {
        m_ctrl_down = true;
        return false;
    }

    if ((state & Gdk::ModifierType::CONTROL_MASK) == Gdk::ModifierType::NO_MODIFIER_MASK)
        return false;

    switch (keyval)
    {
        case GDK_KEY_Left:
            on_btnPrev_clicked();
            return true;
        case GDK_KEY_Right:
            on_btnNext_clicked();
            return true;
        case GDK_KEY_a:
            if (!approve_enabled())
                return false;
            on_btnApprove_clicked();
            return true;
        case GDK_KEY_d:
            if (!disable_enabled())
                return false;
            on_btnDisable_clicked();
            return true;
        case GDK_KEY_space:
            on_btnIgnore_clicked();
            return true;
        case GDK_KEY_s:
            on_btnSave_clicked();
            return true;
        case GDK_KEY_c:
            copy_test_path();
            return true;
        default:
            return false;
    }
}

void MainWindow::on_key_released(guint keyval, guint /*keycode*/, Gdk::ModifierType /*state*/)
{
    if (keyval == GDK_KEY_Control_L || keyval == GDK_KEY_Control_R)
        m_ctrl_down = false;
}

void MainWindow::setup_scroll_sync()
{
    auto left_h = m_scrollLeft.get_hadjustment();
    auto left_v = m_scrollLeft.get_vadjustment();
    auto right_h = m_scrollRight.get_hadjustment();
    auto right_v = m_scrollRight.get_vadjustment();

    link_adjustments(left_h, right_h);
    link_adjustments(right_h, left_h);
    link_adjustments(left_v, right_v);
    link_adjustments(right_v, left_v);
}

void MainWindow::link_adjustments(const Glib::RefPtr<Gtk::Adjustment>& src, const Glib::RefPtr<Gtk::Adjustment>& dst)
{
    src->signal_value_changed().connect([this, src, dst] {
        if (m_syncing || m_ctrl_down)
            return;
        m_syncing = true;
        dst->set_value(src->get_value());
        m_syncing = false;
    });
}

void MainWindow::worker_loop()
{
    // One-time chrome init.
    {
        std::string ce = m_chrome.init(m_repo, m_cache_dir);
        post_to_main([this, ce] {
            m_chrome_error = ce;
            update_title();
            request_renders();
        });
    }

    while (true)
    {
        // Docker (re)init if requested.
        if (m_need_init.exchange(false))
        {
            std::string folder;
            {
                std::lock_guard<std::mutex> lk(m_init_mutex);
                folder = m_init_tests_folder;
            }
            m_docker.shutdown();
            std::string de = m_docker.init(folder, m_litehtml_folder, m_repo, m_cache_dir);
            post_to_main([this, de] {
                m_docker_error = de;
                update_title();
                request_renders();
            });
            continue;
        }

        render_job job;
        {
            std::unique_lock<std::mutex> lk(m_job_mutex);
            m_job_cv.wait(lk, [this] { return m_worker_stop || m_job.valid || m_need_init.load(); });
            if (m_worker_stop)
                return;
            if (m_need_init.load())
                continue;
            job = m_job;
            m_job.valid = false;
        }

        // Step 1: render2png
        std::string r2png_err;
        if (job.need_render2png && m_docker.ready())
        {
            if (!fs::exists(job.r2png_out))
            {
                fs::create_directories(fs::path(job.r2png_out).parent_path());
                r2png_err = m_docker.render(job.html, job.r2png_out);
                if (!r2png_err.empty())
                    std::cerr << "[render2png error] " << job.html << "\n" << r2png_err << "\n";
            }
        }
        post_to_main([this, job, r2png_err] {
            if (job.gen != m_generation)
                return;
            if (r2png_err.empty())
                m_render_errors.erase(job.r2png_out);
            else
                m_render_errors[job.r2png_out] = r2png_err;
            refresh_images();
        });

        // Step 2: chrome (uses render2png output width as the viewport width)
        std::string chrome_err;
        if (job.need_chrome && m_chrome.ready() && m_docker.ready())
        {
            if (!fs::exists(job.chrome_out))
            {
                int w = 0, h = 0;
                if (read_png_size(job.r2png_out, w, h))
                {
                    fs::create_directories(fs::path(job.chrome_out).parent_path());
                    chrome_err = m_chrome.render(job.html, w, h, job.chrome_out);
                }
                else
                {
                    chrome_err = "chrome: cannot read render2png dimensions";
                }
                if (!chrome_err.empty())
                    std::cerr << "[chrome error] " << job.html << "\n" << chrome_err << "\n";
            }
        }
        post_to_main([this, job, chrome_err] {
            if (job.gen != m_generation)
                return;
            if (chrome_err.empty())
                m_render_errors.erase(job.chrome_out);
            else
                m_render_errors[job.chrome_out] = chrome_err;
            refresh_images();
        });
    }
}

std::string MainWindow::rel_of(const std::string& path, const std::string& base)
{
    std::error_code ec;
    auto rel = fs::relative(path, base, ec);
    if (ec)
        return fs::path(path).filename().string();
    return rel.string();
}

std::string MainWindow::render2png_cache_path(const test_entry& t) const
{
    return (fs::path(m_cache_dir) / "render2png" / rel_of(t.base, m_tests_folder)).string() + ".png";
}

std::string MainWindow::chrome_cache_path(const test_entry& t) const
{
    return (fs::path(m_cache_dir) / "chrome" / rel_of(t.base, m_tests_folder)).string() + ".png";
}

std::string MainWindow::approved_cache_path(const test_entry& t) const
{
    return (fs::path(m_cache_dir) / "approved" / rel_of(t.base, m_tests_folder)).string() + ".png";
}

void MainWindow::scan_tests()
{
    m_tests.clear();
    auto files = test_model::scan(m_tests_folder, m_mode);
    for (auto& f : files)
        m_tests.emplace_back(f);
    m_current = -1;
    show_next_test();
    update_title();
    update_buttons();
}

void MainWindow::load_current_test()
{
    invalidate_async();
    const auto& t = m_tests[m_current];
    m_labelFile.set_text("[ " + t.status() + " ] " + t.html);
    refresh_images();
    request_renders();
    update_buttons();
}

void MainWindow::show_next_test()
{
    if (index_is_valid(m_current + 1))
    {
        m_current++;
        load_current_test();
    }
}

void MainWindow::invalidate_async()
{
    m_generation++;
}

void MainWindow::refresh_images()
{
    if (!index_is_valid(m_current))
    {
        m_imageLeft.set_paintable(nullptr);
        m_imageRight.set_paintable(nullptr);
        return;
    }
    const auto& t = m_tests[m_current];
    render_left_image(t);
    render_right_image(t);
    update_buttons();
}

void MainWindow::render_left_image(const test_entry& t)
{
    std::string path;
    std::string label;
    if (m_left == left_source::golden)
    {
        path = t.golden_path();
        label = "Golden";
    }
    else if (m_left == left_source::render2png)
    {
        path = render2png_cache_path(t);
        label = "render2png";
    }
    else
    {
        path = chrome_cache_path(t);
        label = "Chrome";
    }
    set_image(m_imageLeft, path);
    m_frameLeft.set_label(frame_label(label, path));
}

void MainWindow::render_right_image(const test_entry& t)
{
    std::string path;
    std::string label;
    if (m_right == right_source::failed)
    {
        path = t.failed_path();
        label = "FAILED";
    }
    else
    {
        path = chrome_cache_path(t);
        label = "Chrome";
    }
    set_image(m_imageRight, path);
    m_frameRight.set_label(frame_label(label, path));
}

std::string MainWindow::frame_label(const std::string& label, const std::string& path) const
{
    std::string result = label + ": " + filename_of(path) + " " + dims_of(path);
    auto it = m_render_errors.find(path);
    if (it != m_render_errors.end())
        result += "  [ERROR: " + first_line(it->second) + "]";
    return result;
}

void MainWindow::set_image(Gtk::Picture& img, const std::string& path)
{
    if (path.empty() || !fs::exists(path))
    {
        img.set_paintable(nullptr);
        return;
    }
    try
    {
        img.set_paintable(Gdk::Texture::create_from_filename(path));
    }
    catch (const Glib::Error&)
    {
        img.set_paintable(nullptr);
    }
}

void MainWindow::request_renders()
{
    if (!index_is_valid(m_current))
        return;
    const test_entry& t = m_tests[m_current];
    bool need_chrome = (m_right == right_source::chrome) || (m_left == left_source::chrome);
    bool need_r2png = (m_left == left_source::render2png) || need_chrome;

    std::string r2png_out = render2png_cache_path(t);
    std::string chrome_out = chrome_cache_path(t);
    bool r2png_missing = need_r2png && !fs::exists(r2png_out);
    bool chrome_missing = need_chrome && !fs::exists(chrome_out);
    if (!r2png_missing && !chrome_missing)
        return;

    if (!m_docker.ready())
        return;
    if (chrome_missing && !m_chrome.ready())
        return;

    {
        std::lock_guard<std::mutex> lk(m_job_mutex);
        m_job = render_job{t.html, r2png_out, chrome_out, m_generation, need_r2png, need_chrome, true};
    }
    m_job_cv.notify_one();
}

void MainWindow::update_buttons()
{
    m_btnPrev.set_sensitive(index_is_valid(m_current - 1));
    m_btnNext.set_sensitive(index_is_valid(m_current + 1));
    m_btnCopy.set_sensitive(index_is_valid(m_current));
    m_btnApprove.set_sensitive(approve_enabled());
    m_btnDisable.set_sensitive(disable_enabled());
    m_btnSave.set_sensitive(has_pending_changes());
}

bool MainWindow::approve_enabled() const
{
    if (!index_is_valid(m_current))
        return false;
    const test_entry& t = m_tests[m_current];
    if (t.action == test_action::approve)
        return false;
    if (m_right == right_source::failed)
        return fs::exists(t.failed_path());
    // render2png source is only offered in Passed and Disabled modes.
    if (m_left == left_source::render2png)
        return fs::exists(render2png_cache_path(t));
    return false;
}

bool MainWindow::disable_enabled() const
{
    if (!index_is_valid(m_current))
        return false;
    const test_entry& t = m_tests[m_current];
    if (t.action == test_action::disable)
        return false;
    // On-disk disabled (filename starts with '-') can't be disabled again,
    // unless it is marked approve (then Disable undoes that approval).
    if (t.html != t.base && t.action != test_action::approve)
        return false;
    return true;
}

bool MainWindow::has_pending_changes() const
{
    for (const auto& t : m_tests)
    {
        if (t.action == test_action::approve || t.action == test_action::disable)
            return true;
    }
    return false;
}

void MainWindow::update_title()
{
    std::stringstream ss;
    int ni = 0, na = 0, nd = 0;
    for (const auto& t : m_tests)
    {
        switch (t.action)
        {
            case test_action::ignore:
                ni++;
                break;
            case test_action::approve:
                na++;
                break;
            case test_action::disable:
                nd++;
                break;
        }
    }
    ss << "I: " << std::to_string(ni) << "  A: " << std::to_string(na) << "  D: " << std::to_string(nd);
    if (!m_docker.ready())
        ss << "  [render2png " << (m_docker_error == "building" ? "building..." : "unavailable") << "]";
    else if (!m_chrome.ready())
        ss << "  [chrome unavailable]";
    m_labelTitle.set_text(ss.str());
}

void MainWindow::show_error(const std::string& msg)
{
    auto dlg = Gtk::AlertDialog::create("Error");
    dlg->set_detail(msg);
    dlg->show(*this);
}

void MainWindow::restart_renderers()
{
    m_docker_error = "building";
    {
        std::lock_guard<std::mutex> lk(m_init_mutex);
        m_init_tests_folder = m_tests_folder;
    }
    {
        std::lock_guard<std::mutex> lk(m_job_mutex);
        m_job.valid = false;
    }
    m_need_init.store(true);
    m_job_cv.notify_all();
    update_title();
}

void MainWindow::on_mode_changed()
{
    if (m_in_update)
        return;
    int idx = m_comboMode.get_active_row_number();
    if (idx < 0 || idx > 2)
        return;
    test_mode new_mode = static_cast<test_mode>(idx);
    if (new_mode == m_mode)
        return;
    m_mode = new_mode;

    switch (m_mode)
    {
        case test_mode::failed:
            m_left = left_source::golden;
            m_right = right_source::failed;
            break;
        case test_mode::passed:
            m_left = left_source::golden;
            m_right = right_source::chrome;
            break;
        case test_mode::disabled:
            m_left = left_source::render2png;
            m_right = right_source::chrome;
            break;
    }
    m_in_update = true;
    update_left_button_label();
    m_btnLeftGolden.set_active(m_left == left_source::golden);
    m_btnLeftSecond.set_active(m_left != left_source::golden);
    m_btnRightFailed.set_active(m_right == right_source::failed);
    m_btnRightChrome.set_active(m_right == right_source::chrome);
    m_in_update = false;

    scan_tests();
}

void MainWindow::update_left_button_label()
{
    m_btnLeftSecond.set_label(m_mode == test_mode::failed ? "Chrome" : "render2png");
}

void MainWindow::on_left_source_changed()
{
    if (m_in_update)
        return;
    if (m_btnLeftGolden.get_active())
        m_left = left_source::golden;
    else
        m_left = (m_mode == test_mode::failed) ? left_source::chrome : left_source::render2png;
    refresh_images();
    request_renders();
    update_buttons();
}

void MainWindow::on_right_source_changed()
{
    if (m_in_update)
        return;
    m_right = m_btnRightChrome.get_active() ? right_source::chrome : right_source::failed;
    refresh_images();
    request_renders();
    update_buttons();
}

void MainWindow::on_btnFolder_clicked()
{
    auto dlg = Gtk::FileDialog::create();
    dlg->set_title("Select tests folder");
    dlg->select_folder(*this, [this, dlg](const Glib::RefPtr<Gio::AsyncResult>& result) {
        Glib::RefPtr<Gio::File> file;
        try
        {
            file = dlg->select_folder_finish(result);
        }
        catch (const Glib::Error&)
        {
            return;
        }
        if (!file)
            return;
        std::string path = file->get_path();
        if (path.empty())
            return;
        m_tests_folder = fs::absolute(path).lexically_normal().string();
        restart_renderers();
        scan_tests();
    });
}

void MainWindow::copy_test_path()
{
    if (!index_is_valid(m_current))
        return;
    auto display = Gdk::Display::get_default();
    if (display)
        display->get_clipboard()->set_text(m_tests[m_current].html);
}

void MainWindow::on_btnPrev_clicked()
{
    if (index_is_valid(m_current - 1))
    {
        m_current--;
        load_current_test();
    }
}

void MainWindow::on_btnNext_clicked()
{
    show_next_test();
}

void MainWindow::on_btnApprove_clicked()
{
    if (!index_is_valid(m_current))
        return;
    auto& t = m_tests[m_current];
    bool from_render2png = (m_right != right_source::failed);
    t.action = test_action::approve;
    t.approve_from_render2png = from_render2png;
    if (from_render2png)
    {
        std::string src = render2png_cache_path(t);
        std::string dst = approved_cache_path(t);
        std::error_code ec;
        fs::create_directories(fs::path(dst).parent_path());
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    }
    update_title();
    update_buttons();
    show_next_test();
}

void MainWindow::on_btnDisable_clicked()
{
    if (!index_is_valid(m_current))
        return;
    m_tests[m_current].action = test_action::disable;
    update_title();
    update_buttons();
    show_next_test();
}

void MainWindow::on_btnIgnore_clicked()
{
    if (!index_is_valid(m_current))
        return;
    m_tests[m_current].action = test_action::ignore;
    update_title();
    update_buttons();
    show_next_test();
}

void MainWindow::on_btnSave_clicked()
{
    for (const auto& t : m_tests)
    {
        if (t.action == test_action::disable)
        {
            std::string fname = fs::path(t.html).filename().string();
            if (!fname.empty() && fname[0] != '-')
            {
                std::string to = (fs::path(t.html).parent_path() / ("-" + fname)).string();
                std::error_code ec;
                fs::rename(t.html, to, ec);
                if (ec)
                    show_error(ec.message());
            }
        }
        else if (t.action == test_action::approve)
        {
            std::string new_html = t.base;
            if (new_html != t.html)
            {
                std::error_code ec;
                fs::rename(t.html, new_html, ec);
                if (ec)
                    show_error(ec.message());
            }

            std::string golden = t.golden_path();
            if (t.approve_from_render2png)
            {
                std::string src = approved_cache_path(t);
                if (!fs::exists(src))
                    src = render2png_cache_path(t);
                if (fs::exists(src))
                {
                    std::error_code ec;
                    fs::copy_file(src, golden, fs::copy_options::overwrite_existing, ec);
                    if (ec)
                        show_error(ec.message());
                }
            }
            else
            {
                std::string failed = t.failed_path();
                if (fs::exists(failed))
                {
                    std::error_code ec;
                    fs::rename(failed, golden, ec);
                    if (ec)
                        show_error(ec.message());
                }
            }
        }
    }
    scan_tests();
}

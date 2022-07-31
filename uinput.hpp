#include <glib.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <chrono>
#include <thread>
#include <future>
#include <filesystem>
#include <vector>
#include <iostream>
#include <map>
#include "imu/imu.h"

struct Event
{
    int type;
    int code;
    int value;
};

class UInput
{
private:
    bool gyro_switch;
    bool js_switch;
    
    int auto_update_gyro_thread_id;
    int auto_send_rel_thread_id;
    int left_fn_single_click_thread_id;
    int right_fn_single_click_thread_id;

    int mouse_rel_x, mouse_rel_y;
    int abs_rx, abs_ry;
    float v_yaw, v_pitch;
    int gyro_deadzone;

    //input devices
    libevdev* src_dev;
    libevdev* fn_dev;
    IMU* imu;
    //output devices
    libevdev_uinput* target_dev;
    libevdev_uinput* mouse_dev;
    int src_fd, fn_fd, target_fd;
    GMainLoop* g_main;
    std::vector<Event> src_event_queue, fn_event_queue;

public:
    UInput(libevdev* src_dev, 
            libevdev* fn_dev, 
            IMU* imu,
            libevdev_uinput* target_dev,
            libevdev_uinput* mouse_dev,
            int gyro_deadzone);
    ~UInput();
    void run();
    bool parse_as_js(const struct input_event& ev, std::vector<Event>& event_queue);
    bool parse_as_mouse(const struct input_event& ev, std::vector<Event>& event_queue);
    bool parse_fn(const struct input_event& ev, std::vector<Event>& event_queue);
    bool submit_msg(libevdev_uinput* ui_dev, std::vector<Event>& event_queue);
    gboolean on_read_from_src(GIOChannel* source, GIOCondition condition);
    static gboolean on_read_from_src_wrap(GIOChannel* source,
                                    GIOCondition condition,
                                    gpointer userdata)
    {
        return static_cast<UInput*>(userdata)->on_read_from_src(source, condition);
    }
    gboolean on_read_from_fn(GIOChannel* source, GIOCondition condition);
    static gboolean on_read_from_fn_wrap(GIOChannel* source,
                                    GIOCondition condition,
                                    gpointer userdata)
    {
        return static_cast<UInput*>(userdata)->on_read_from_fn(source, condition);
    }
    gboolean on_read_from_target(GIOChannel* source, GIOCondition condition);
    static gboolean on_read_from_target_wrap(GIOChannel* source,
                                    GIOCondition condition,
                                    gpointer userdata)
    {
        return static_cast<UInput*>(userdata)->on_read_from_target(source, condition);
    }
    gboolean left_fn_single_click();
    static gboolean left_fn_single_click_wrap(gpointer userdata)
    {
        return static_cast<UInput*>(userdata)->left_fn_single_click();
    }
    bool left_fn_double_click();
    gboolean right_fn_single_click();
    static gboolean right_fn_single_click_wrap(gpointer userdata)
    {
        return static_cast<UInput*>(userdata)->right_fn_single_click();
    }
    bool right_fn_double_click();
    bool left_right_fn_click();
    gboolean auto_send_rel();
    static gboolean auto_send_rel_wrap(gpointer userdata)
    {
        return static_cast<UInput*>(userdata)->auto_send_rel();
    }
    gboolean auto_update_gyro();
    static gboolean auto_update_gyro_wrap(gpointer userdata)
    {
        return static_cast<UInput*>(userdata)->auto_update_gyro();
    }
};

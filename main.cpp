#include "imu/imu.h"
#include <linux/input.h>
#include <linux/uinput.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <chrono>
#include <thread>
#include <future>
#include <filesystem>

using namespace::std::literals;

libevdev* get_dev_by_name(std::string name)
{
    auto input_root_path = "/dev/input/";
    struct libevdev *dev;
    for (const auto & file : std::filesystem::directory_iterator(input_root_path))
    {
        int tmp_fd = open(file.path().c_str(), O_RDONLY|O_NONBLOCK);
        auto err = libevdev_new_from_fd(tmp_fd, &dev);
        if (err == 0)
        {
            auto dev_name = std::string(libevdev_get_name(dev));
            if (dev_name == name)
            {
                return dev;
            } else {
                libevdev_free(dev);
            }
        }
        close(tmp_fd);        
    }
    return nullptr;
    
}

float linear_range_interp(float min, float max, float target_min, float target_max, float val)
{
    return (abs(val) - min)/(max - min) * (target_max - target_min) + target_min;

}

void send_raxis(libevdev_uinput* dev, 
                float rx, float ry, 
                float gyro_yaw, float gyro_pitch, 
                float scale_factor, bool use_gyro)
{
    auto gyro_norm = sqrt(pow(gyro_yaw,2) + pow(gyro_pitch,2));
    auto scaled_gyro_norm = linear_range_interp(0.1, 1.8, 9500, 32000, gyro_norm);
    auto scaled_gyro_yaw = gyro_yaw / gyro_norm * scaled_gyro_norm;
    auto scaled_gyro_pitch = gyro_pitch / gyro_norm * scaled_gyro_norm;
    // std::cout << "raw: " << gyro_yaw << " scaled: " << scaled_gyro_yaw << std::endl;
    rx += use_gyro ? scaled_gyro_yaw : 0;
    ry += use_gyro ? scaled_gyro_pitch : 0;
    libevdev_uinput_write_event(dev, EV_ABS, ABS_RX, rx);
    libevdev_uinput_write_event(dev, EV_SYN, SYN_REPORT, 0);
    libevdev_uinput_write_event(dev, EV_ABS, ABS_RY, ry);
    libevdev_uinput_write_event(dev, EV_SYN, SYN_REPORT, 0);
}

void send_key_event(libevdev_uinput* dev, unsigned int code, int value)
{
    libevdev_uinput_write_event(dev, EV_KEY, code, value);
    libevdev_uinput_write_event(dev, EV_SYN, SYN_REPORT, 0);
}

int main(int argc, char const *argv[])
{
    int err;
    struct libevdev *dev;
    struct libevdev *src_dev;
    struct libevdev *fn_dev;
    struct libevdev_uinput *uidev;
    dev = libevdev_new();
    // grab source gamepad and fn input device
    src_dev = get_dev_by_name("Microsoft X-Box 360 pad");
    fn_dev = get_dev_by_name("AT Translated Set 2 keyboard");
    err = libevdev_grab(src_dev, LIBEVDEV_GRAB);
    if (err != 0)
    {
        std::cout << "grab src input err!" << std::endl;
        return err;
    }
    err = libevdev_grab(fn_dev, LIBEVDEV_GRAB);
    if (err != 0)
    {
        std::cout << "grab fn input err!" << std::endl;
        return err;
    }
    libevdev_enable_event_code(src_dev, EV_KEY, KEY_VOLUMEDOWN, NULL);
    libevdev_enable_event_code(src_dev, EV_KEY, KEY_VOLUMEUP, NULL);
    libevdev_enable_event_code(src_dev, EV_KEY, BTN_MODE, NULL);
    // create virtual input device as proxy
    err = libevdev_uinput_create_from_device(src_dev,
            LIBEVDEV_UINPUT_OPEN_MANAGED,
            &uidev);
    if (err != 0)
    {
        std::cout << "create uinput dev err!" << std::endl;
        return err;
    }
    // init IMU with filter, wait one sec to let IMU finish self-calib
    auto imu = IMU();
    sleep(1);
    int rc = 1;
    float scale_factor = 50000;
    bool use_gyro = false;

    std::jthread* gyro_steam_switch_thread = NULL;
    std::jthread* keyboard_quick_menu_switch_thread = NULL;
    bool gyro_steam_switch_thread_done = true, keyboard_quick_menu_switch_thread_done = true;
    int rx_val = 0, ry_val = 0;
    std::cout << "start loop" << std::endl;
    double v_yaw = 0, v_pitch = 0;  
    while(true)
    {
        struct input_event ev;
        // for source gamepad
        rc = libevdev_next_event(src_dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        if (rc == 0)
        {
            if (ev.code == ABS_RX || ev.code == ABS_RY) {
                // interplate axis
                if (ev.code == ABS_RX)
                    rx_val = ev.value;
                else
                    ry_val = ev.value;
                auto v = imu.getMotion();
                v_yaw = 0.8 * v_yaw + 0.2 * v.yaw;
                v_pitch = 0.8 * v_pitch + 0.2 * v.pitch;
                send_raxis(uidev, rx_val, ry_val, v_yaw, v_pitch, scale_factor, use_gyro);
            } else if (ev.type != EV_SYN) {
                // passthrough
                libevdev_uinput_write_event(uidev, ev.type, ev.code, ev.value);
                libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
            } else {
                // SYN, sleep
                usleep(10*1e3);
            }
        } else if (rc == -EAGAIN) {
            // no event, send gyro and sleep
            // std::cout << "update gyro: " << use_gyro << std::endl;
            auto v = imu.getMotion();
            v_yaw = 0.8 * v_yaw + 0.2 * v.yaw;
            v_pitch = 0.8 * v_pitch + 0.2 * v.pitch;
            send_raxis(uidev, rx_val, ry_val, v_yaw, v_pitch, scale_factor, use_gyro);
            usleep(10*1e3);
        } else {
            // for LIBEVDEV_READ_STATUS_SYNC, skip
            usleep(10*1e3);
        }
        // for fn keys, enable/disable gyro, volume up/down, quick menu,
        // steam, virtual keyboard
        rc = libevdev_next_event(fn_dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        if (rc == 0)
        {
            if (ev.type == EV_KEY)
            {
                if (ev.code == KEY_D && ev.value == 1) // left-bottom button
                {
                    // try clean up body if task is done
                    if (gyro_steam_switch_thread_done && gyro_steam_switch_thread != NULL) {
                        gyro_steam_switch_thread->join();
                        delete gyro_steam_switch_thread;
                        gyro_steam_switch_thread = NULL;
                        gyro_steam_switch_thread_done = false;
                    }
                    if (gyro_steam_switch_thread == NULL)
                    {
                        gyro_steam_switch_thread = new std::jthread([uidev, &use_gyro, &gyro_steam_switch_thread_done](std::stop_token stoken){
                                                    gyro_steam_switch_thread_done = false;
                                                    std::this_thread::sleep_for(0.5s);
                                                    if (stoken.stop_requested()) {
                                                        // double click: gyro switch
                                                        use_gyro = !use_gyro;
                                                    } else {
                                                        // single click: steam menu
                                                        send_key_event(uidev, BTN_MODE, 1);
                                                        send_key_event(uidev, BTN_MODE, 0);
                                                    }
                                                    gyro_steam_switch_thread_done = true;
                                                });
                        
                    } else {
                        // second click stop switch thread
                        gyro_steam_switch_thread->request_stop();
                        gyro_steam_switch_thread->join();
                        delete gyro_steam_switch_thread;
                        gyro_steam_switch_thread = NULL;
                    }
                    
                    
                } else if (ev.code == KEY_O && ev.value == 1) // right-bottom button
                {
                    // try clean up body if task is done
                    if (keyboard_quick_menu_switch_thread_done && keyboard_quick_menu_switch_thread != NULL) {
                        keyboard_quick_menu_switch_thread->join();
                        delete keyboard_quick_menu_switch_thread;
                        keyboard_quick_menu_switch_thread = NULL;
                        keyboard_quick_menu_switch_thread_done = false;
                    }
                    if (keyboard_quick_menu_switch_thread == NULL)
                    {
                        keyboard_quick_menu_switch_thread = new std::jthread([uidev, &use_gyro, &keyboard_quick_menu_switch_thread_done](std::stop_token stoken){
                                                    keyboard_quick_menu_switch_thread_done = false;
                                                    std::this_thread::sleep_for(0.5s);
                                                    if (stoken.stop_requested()) {
                                                        // double click: on-screen keyboard
                                                        send_key_event(uidev, BTN_MODE, 1);
                                                        usleep(100*1e3);
                                                        send_key_event(uidev, BTN_NORTH, 1);
                                                        usleep(100*1e3);
                                                        send_key_event(uidev, BTN_NORTH, 0);
                                                        usleep(100*1e3);
                                                        send_key_event(uidev, BTN_MODE, 0);
                                                        usleep(100*1e3);
                                                    } else {
                                                        // single click: quick menu
                                                        send_key_event(uidev, BTN_MODE, 1);
                                                        usleep(100*1e3);
                                                        send_key_event(uidev, BTN_SOUTH, 1);
                                                        usleep(100*1e3);
                                                        send_key_event(uidev, BTN_SOUTH, 0);
                                                        usleep(100*1e3);
                                                        send_key_event(uidev, BTN_MODE, 0);
                                                        usleep(100*1e3);
                                                    }
                                                    keyboard_quick_menu_switch_thread_done = true;
                                                });
                    } else {
                        // second click stop switch thread
                        keyboard_quick_menu_switch_thread->request_stop();
                        keyboard_quick_menu_switch_thread->join();
                        delete keyboard_quick_menu_switch_thread;
                        keyboard_quick_menu_switch_thread = NULL;
                    }
                } else if (ev.code == KEY_VOLUMEDOWN || ev.code == KEY_VOLUMEUP)
                {
                    // volume up/down, pass through
                    libevdev_uinput_write_event(uidev, ev.type, ev.code, ev.value);
                    libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
                }
            }
        }
    }
    
    return 0;
}

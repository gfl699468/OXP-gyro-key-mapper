#include "imu/imu.h"
#include <glib.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <chrono>
#include <filesystem>
#include "uinput.hpp"

libevdev *get_dev_by_name(std::string name)
{
    auto input_root_path = "/dev/input/";
    struct libevdev *dev;
    for (const auto &file : std::filesystem::directory_iterator(input_root_path))
    {
        int tmp_fd = open(file.path().c_str(), O_RDONLY | O_NONBLOCK);
        auto err = libevdev_new_from_fd(tmp_fd, &dev);
        if (err == 0)
        {
            auto dev_name = std::string(libevdev_get_name(dev));
            if (dev_name == name)
                return dev;
            else
                libevdev_free(dev);
        }
        close(tmp_fd);
    }
    return nullptr;
}

libevdev_uinput *create_uinput_dev(std::string name,
                                   libevdev *ref_dev,
                                   std::map<int, std::vector<int>> code_list,
                                   std::map<int, const input_absinfo *> absinfo_map)
{
    struct libevdev *dev;
    struct libevdev_uinput *uidev;
    dev = libevdev_new();
    libevdev_set_name(dev, name.c_str());
    if (ref_dev != nullptr)
    {
        libevdev_set_id_bustype(dev, libevdev_get_id_bustype(ref_dev));
        libevdev_set_id_vendor(dev, libevdev_get_id_vendor(ref_dev));
        libevdev_set_id_version(dev, libevdev_get_id_version(ref_dev));
        libevdev_set_id_product(dev, libevdev_get_id_product(ref_dev));
    }

    for (const auto &ev_pair : code_list)
    {
        libevdev_enable_event_type(dev, ev_pair.first);
        for (const auto &ev_code : ev_pair.second)
            libevdev_enable_event_code(dev, ev_pair.first, ev_code, NULL);
    }
    if (!absinfo_map.empty())
    {
        libevdev_enable_event_type(dev, EV_ABS);
        for (const auto &abs_pair : absinfo_map)
            libevdev_enable_event_code(dev, EV_ABS, abs_pair.first, abs_pair.second);
    }
    auto ret = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
    return uidev;
}

int main(int argc, char const *argv[])
{
    int err;
    struct libevdev *dev;
    struct libevdev *src_dev;
    struct libevdev *fn_dev;

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

    auto gamepad_uidev = create_uinput_dev("Virtual XBox360", src_dev,
                                           {{EV_KEY, {BTN_NORTH, BTN_SOUTH, BTN_WEST, BTN_EAST, BTN_TL, BTN_TR, BTN_SELECT, BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR}},
                                            {EV_SYN, {}},
                                            {EV_FF, {FF_RUMBLE, FF_PERIODIC, FF_SQUARE, FF_TRIANGLE, FF_SINE, FF_GAIN}}},
                                           {{ABS_X, libevdev_get_abs_info(src_dev, ABS_X)},
                                            {ABS_Y, libevdev_get_abs_info(src_dev, ABS_Y)},
                                            {ABS_Z, libevdev_get_abs_info(src_dev, ABS_Z)},
                                            {ABS_RX, libevdev_get_abs_info(src_dev, ABS_RX)},
                                            {ABS_RY, libevdev_get_abs_info(src_dev, ABS_RY)},
                                            {ABS_RZ, libevdev_get_abs_info(src_dev, ABS_RZ)},
                                            {ABS_HAT0X, libevdev_get_abs_info(src_dev, ABS_HAT0X)},
                                            {ABS_HAT0Y, libevdev_get_abs_info(src_dev, ABS_HAT0Y)}});

    auto mouse_uidev = create_uinput_dev("Virtual Mouse", nullptr,
                                         {{EV_KEY, {BTN_LEFT, BTN_MIDDLE, BTN_RIGHT, KEY_VOLUMEDOWN, KEY_VOLUMEUP}},
                                          {EV_SYN, {}},
                                          {EV_REL, {REL_X, REL_Y, REL_WHEEL, REL_WHEEL_HI_RES}}},
                                         {});

    // init IMU with filter, wait one sec to let IMU finish self-calib
    IMU* imu = new IMU();
    sleep(1);
    int rc = 1;
    float scale_factor = 50000;

    auto uinput_handler = UInput(src_dev, fn_dev, imu, gamepad_uidev, mouse_uidev, 9000);
    uinput_handler.run();
}

#include "uinput.hpp"

float linear_range_interp(float min, float max, float target_min, float target_max, float val)
{
    return (abs(val) - min)/(max - min) * (target_max - target_min) + target_min;

}

UInput::UInput(libevdev *src_dev,
               libevdev *fn_dev,
               IMU *imu,
               libevdev_uinput *target_dev,
               libevdev_uinput *mouse_dev,
               int gyro_deadzone) : src_dev(src_dev),
                                             fn_dev(fn_dev),
                                             imu(imu),
                                             target_dev(target_dev),
                                             mouse_dev(mouse_dev),
                                             js_switch(true),
                                             gyro_switch(false),
                                             mouse_rel_x(0),
                                             mouse_rel_y(0),
                                             abs_rx(0),
                                             abs_ry(0),
                                             v_yaw(0),
                                             v_pitch(0),
                                             gyro_deadzone(gyro_deadzone),
                                             left_fn_single_click_thread_id(0),
                                             right_fn_single_click_thread_id(0)
{
  src_fd = libevdev_get_fd(src_dev);
  fn_fd = libevdev_get_fd(fn_dev);
  target_fd = libevdev_uinput_get_fd(target_dev);
  // init callback for src_dev
  // read event from src dev and send to target dev
  {
    // start g_io_channel
    auto m_io_channel = g_io_channel_unix_new(libevdev_get_fd(src_dev));

    // set encoding to binary
    GError *error = NULL;
    if (g_io_channel_set_encoding(m_io_channel, NULL, &error) != G_IO_STATUS_NORMAL)
    {
      std::cout << error->message << std::endl;
      g_error_free(error);
    }

    g_io_channel_set_buffered(m_io_channel, false);

    g_io_add_watch(m_io_channel,
                   static_cast<GIOCondition>(G_IO_IN | G_IO_ERR | G_IO_HUP),
                   &UInput::on_read_from_src_wrap, this);
  }
  // init callback for fn_dev
  // read event from fn dev and send to target dev
  {
    // start g_io_channel
    auto m_io_channel = g_io_channel_unix_new(libevdev_get_fd(fn_dev));

    // set encoding to binary
    GError *error = NULL;
    if (g_io_channel_set_encoding(m_io_channel, NULL, &error) != G_IO_STATUS_NORMAL)
    {
      std::cout << error->message << std::endl;
      g_error_free(error);
    }

    g_io_channel_set_buffered(m_io_channel, false);

    g_io_add_watch(m_io_channel,
                   static_cast<GIOCondition>(G_IO_IN | G_IO_ERR | G_IO_HUP),
                   &UInput::on_read_from_fn_wrap, this);
  }

  // // init callback for target_dev
  // // read ff event and send to src dev
  // {
  //     // start g_io_channel
  //     auto m_io_channel = g_io_channel_unix_new(libevdev_uinput_get_fd(target_dev));

  //     // set encoding to binary
  //     GError* error = NULL;
  //     if (g_io_channel_set_encoding(m_io_channel, NULL, &error) != G_IO_STATUS_NORMAL)
  //     {
  //     std::cout<< error->message << std::endl;
  //     g_error_free(error);
  //     }

  //     g_io_channel_set_buffered(m_io_channel, false);

  //     g_io_add_watch(m_io_channel,
  //                     static_cast<GIOCondition>(G_IO_IN | G_IO_ERR | G_IO_HUP),
  //                     &UInput::on_read_from_target_wrap, this);
  // }
}

UInput::~UInput(){};

void UInput::run()
{
  g_main = g_main_loop_new(NULL, false);
  g_main_loop_run(g_main);
}

gboolean UInput::on_read_from_fn(GIOChannel *source, GIOCondition condition)
{
  // read data
  struct input_event ev[128];
  int rd = 0;
  while ((rd = ::read(fn_fd, ev, sizeof(struct input_event) * 128)) > 0)
  {
    for (size_t i = 0; i < rd / sizeof(struct input_event); ++i)
    {
      if (ev[i].type == EV_SYN)
        submit_msg(target_dev, src_event_queue);
      else
        parse_fn(ev[i], fn_event_queue);
    }
  }

  return TRUE;
}
gboolean UInput::on_read_from_src(GIOChannel *source, GIOCondition condition)
{
  // read data
  struct input_event ev[128];
  int rd = 0;
  while ((rd = ::read(src_fd, ev, sizeof(struct input_event) * 128)) > 0)
  {
    for (size_t i = 0; i < rd / sizeof(struct input_event); ++i)
    {
      if (ev[i].type == EV_SYN)
      {
        // std::cout << "submit ev" << std::endl;
        if (js_switch)
          submit_msg(target_dev, src_event_queue);
        else
          submit_msg(mouse_dev, src_event_queue);
      }
      else
      {
        if (js_switch)
          parse_as_js(ev[i], src_event_queue);
        else
          parse_as_mouse(ev[i], src_event_queue);
      }
    }
  }

  return TRUE;
}
// gboolean on_read_from_target(GIOChannel* source, GIOCondition condition)
// {

// }

bool UInput::parse_as_js(const struct input_event &ev, std::vector<Event> &event_queue)
{
  switch (ev.type)
  {
  case EV_KEY:
  {
    // std::cout << "get key event " << ev.code << std::endl;
    event_queue.emplace_back(Event(ev.type, ev.code, ev.value));
    return true;
  }
  break;

  case EV_ABS:
  {
    if (ev.code == ABS_RX) // cache rx/ry for gyro update
    {
      abs_rx = ev.value;
    } else if (ev.code == ABS_RY)
    {
      abs_ry = ev.value;
    }
    // std::cout << "get abs event " << ev.code << std::endl;
    event_queue.emplace_back(Event(ev.type, ev.code, ev.value));
    return true;
  }
  break;
  default:
    // not supported event
    return false;
    break;
  }
}

bool UInput::parse_as_mouse(const struct input_event &ev, std::vector<Event> &event_queue)
{
  switch (ev.type)
  {
  case EV_KEY:
  {
    // std::cout << "get key event " << ev.code << std::endl;
    switch (ev.code)
    {
    case BTN_SOUTH:
    {
      event_queue.emplace_back(Event(ev.type, BTN_LEFT, ev.value));
    }
    break;
    case BTN_EAST:
    {
      event_queue.emplace_back(Event(ev.type, BTN_RIGHT, ev.value));
    }
    break;
    case BTN_WEST:
    {
      event_queue.emplace_back(Event(ev.type, BTN_MIDDLE, ev.value));
    }
    break;
    default:
      // not supported event
      return false;
      break;
    }
  }
  break;

  case EV_ABS:
  {
    switch (ev.code)
    {
    case ABS_X:
    {
      mouse_rel_x = static_cast<int>(ev.value / 32768.0 * 7);
    }
    break;
    case ABS_Y:
    {
      mouse_rel_y = static_cast<int>(ev.value / 32768.0 * 7);
    }
    break;
    case ABS_RY:
    {
      event_queue.emplace_back(Event(EV_REL, REL_WHEEL, ev.value > 1 ? 1 : -1));
      event_queue.emplace_back(Event(EV_REL, REL_Y, abs(static_cast<int>(ev.value / 32768 * 120))));
    }
    break;
    default:
      return false;
      break;
    }
  }
  break;
  default:
    // not supported event
    return false;
    break;
  }
  return true;
}

/*
trigger mapped action(steam menu) with certain delay to ensure
it's an single click on left fn btn
*/
gboolean UInput::left_fn_single_click()
{
  // std::cout << "left_fn_single_click" << std::endl;
  fn_event_queue.emplace_back(Event(EV_KEY, BTN_MODE, 1));
  fn_event_queue.emplace_back(Event(EV_KEY, BTN_MODE, 0));
  submit_msg(target_dev, fn_event_queue);
  left_fn_single_click_thread_id = 0;
  return 0;
}
bool UInput::left_fn_double_click()
{
  // std::cout << "left_fn_double_click" << std::endl;
  gyro_switch = !gyro_switch;
  if (gyro_switch)
  {
    auto_update_gyro_thread_id = g_timeout_add(10, &UInput::auto_update_gyro_wrap, this);
  } else {
    g_source_remove(auto_update_gyro_thread_id);
    auto_update_gyro_thread_id = 0;
  }
  return 0;
}
/*
trigger mapped action(quick menu) with certain delay to ensure
it's an single click on right fn btn
*/
gboolean UInput::right_fn_single_click()
{
  // std::cout << "right_fn_single_click" << std::endl;
  fn_event_queue.emplace_back(Event(EV_KEY, BTN_MODE, 1));
  fn_event_queue.emplace_back(Event(EV_KEY, BTN_SOUTH, 1));
  submit_msg(target_dev, fn_event_queue);
  usleep(100 * 1e3);
  fn_event_queue.emplace_back(Event(EV_KEY, BTN_SOUTH, 0));
  fn_event_queue.emplace_back(Event(EV_KEY, BTN_MODE, 0));
  submit_msg(target_dev, fn_event_queue);
  right_fn_single_click_thread_id = 0;
  return 0;
}
bool UInput::right_fn_double_click()
{
  // std::cout << "right_fn_double_click" << std::endl;
  fn_event_queue.emplace_back(Event(EV_KEY, BTN_MODE, 1));
  submit_msg(target_dev, fn_event_queue);
  usleep(100 * 1e3);
  fn_event_queue.emplace_back(Event(EV_KEY, BTN_NORTH, 1));
  submit_msg(target_dev, fn_event_queue);
  usleep(100 * 1e3);
  fn_event_queue.emplace_back(Event(EV_KEY, BTN_NORTH, 0));
  submit_msg(target_dev, fn_event_queue);
  usleep(100 * 1e3);
  fn_event_queue.emplace_back(Event(EV_KEY, BTN_MODE, 0));
  submit_msg(target_dev, fn_event_queue);
  return 0;
}
bool UInput::left_right_fn_click()
{
  // std::cout << "left_right_fn_click" << std::endl;
  js_switch = !js_switch;
  if (js_switch) // mouse switch to joystick
  {
    mouse_rel_x = 0;
    mouse_rel_y = 0;
    if (auto_send_rel_thread_id)
    {
      g_source_remove(auto_send_rel_thread_id);
      auto_send_rel_thread_id = 0;
    }
  } else {
    auto_send_rel_thread_id = g_timeout_add(10, &UInput::auto_send_rel_wrap, this);
  }
  return 0;
}
/*
send rel event repeatly if raw rel event's value is not zero
*/
gboolean UInput::auto_send_rel()
{
  if (mouse_rel_x != 0)
    // std::cout << "auto send rel_x " << mouse_rel_x << std::endl;
    src_event_queue.emplace_back(Event(EV_REL, REL_X, mouse_rel_x));
  if (mouse_rel_y != 0)
    // std::cout << "auto send rel_y " << mouse_rel_y << std::endl;
    src_event_queue.emplace_back(Event(EV_REL, REL_Y, mouse_rel_y));
  submit_msg(mouse_dev, src_event_queue);
  return 1;
}

gboolean UInput::auto_update_gyro()
{
  auto v = imu->getMotion();
  v_yaw = 0.8 * v_yaw + 0.2 * v.yaw;
  v_pitch = 0.8 * v_pitch + 0.2 * v.pitch;
  auto gyro_norm = sqrt(pow(v_yaw,2) + pow(v_pitch,2));
  auto abs_norm = sqrt(pow(abs_rx,2) + pow(abs_ry,2));
  
  if (abs_norm <= gyro_deadzone) // abs norm is too small, ignore it
  {
    auto scaled_gyro_norm = linear_range_interp(0.1, 1.8, gyro_deadzone, 22000+gyro_deadzone, gyro_norm);
    auto scaled_gyro_yaw = v_yaw / gyro_norm * scaled_gyro_norm;
    auto scaled_gyro_pitch = v_pitch / gyro_norm * scaled_gyro_norm;
    src_event_queue.emplace_back(Event(EV_ABS, ABS_RX, scaled_gyro_yaw));
    src_event_queue.emplace_back(Event(EV_ABS, ABS_RY, scaled_gyro_pitch));
  } else {
    auto scaled_gyro_norm = linear_range_interp(0.1, 1.8, 0, 22000, gyro_norm);
    auto scaled_gyro_yaw = v_yaw / gyro_norm * scaled_gyro_norm;
    auto scaled_gyro_pitch = v_pitch / gyro_norm * scaled_gyro_norm;
    src_event_queue.emplace_back(Event(EV_ABS, ABS_RX, scaled_gyro_yaw+abs_rx));
    src_event_queue.emplace_back(Event(EV_ABS, ABS_RY, scaled_gyro_pitch+abs_ry));
  }
  submit_msg(target_dev, src_event_queue);
  return 1;
}

bool UInput::parse_fn(const struct input_event &ev, std::vector<Event> &event_queue)
{
  switch (ev.type)
  {
  case EV_KEY:
  {
    // std::cout << "get key event " << ev.code << std::endl;
    switch (ev.code)
    {
    case KEY_D:          // left-bottom
      if (ev.value == 1) // only trigger once
      {
        if (right_fn_single_click_thread_id != 0)
        {
          g_source_remove(right_fn_single_click_thread_id);
          right_fn_single_click_thread_id = 0;
          left_right_fn_click();
        }
        else if (left_fn_single_click_thread_id == 0)
        {
          left_fn_single_click_thread_id = g_timeout_add(1000, left_fn_single_click_wrap, this);
        }
        else
        {
          g_source_remove(left_fn_single_click_thread_id);
          left_fn_single_click_thread_id = 0;
          left_fn_double_click();
        }
      }
      break;
    case KEY_O:          // left-bottom
      if (ev.value == 1) // only trigger once
      {
        if (left_fn_single_click_thread_id != 0)
        {
          g_source_remove(left_fn_single_click_thread_id);
          left_fn_single_click_thread_id = 0;
          left_right_fn_click();
        }
        else if (right_fn_single_click_thread_id == 0)
        {
          right_fn_single_click_thread_id = g_timeout_add(1000, right_fn_single_click_wrap, this);
        }
        else
        {
          g_source_remove(right_fn_single_click_thread_id);
          right_fn_single_click_thread_id = 0;
          right_fn_double_click();
        }
      }
      break;
      case KEY_VOLUMEDOWN: case KEY_VOLUMEUP:
      {
        src_event_queue.emplace_back(Event(EV_KEY, ev.code, ev.value));
      }
      break;
    default:
      break;
    }
    return true;
  }
  break;
  default:
    // not supported event
    return false;
    break;
  }
}

bool UInput::submit_msg(libevdev_uinput *ui_dev, std::vector<Event> &event_queue)
{
  for (auto ev : event_queue)
  {
    auto ret = libevdev_uinput_write_event(ui_dev, ev.type, ev.code, ev.value);
    if (ret != 0)
      std::cout << "error! " << ret << std::endl;
  }
  libevdev_uinput_write_event(ui_dev, EV_SYN, SYN_REPORT, 0);
  event_queue.clear();
  return true;
}
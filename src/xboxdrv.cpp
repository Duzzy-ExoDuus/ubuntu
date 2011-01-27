/* 
**  Xbox/Xbox360 USB Gamepad Userspace Driver
**  Copyright (C) 2008 Ingo Ruhnke <grumbel@gmx.de>
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "xboxdrv.hpp"

#include <boost/format.hpp>
#include <boost/algorithm/string/join.hpp>
#include <iostream>
#include <signal.h>
#include <stdio.h>

#include "command_line_options.hpp"
#include "dummy_message_processor.hpp"
#include "evdev_controller.hpp"
#include "evdev_helper.hpp"
#include "helper.hpp"
#include "raise_exception.hpp"
#include "uinput_message_processor.hpp"
#include "usb_helper.hpp"
#include "word_wrap.hpp"
#include "xbox_controller_factory.hpp"
#include "xboxdrv_daemon.hpp"
#include "xboxdrv_thread.hpp"

// Some ugly global variables, needed for sigint catching
bool global_exit_xboxdrv = false;
XboxGenericController* global_controller = 0;

void on_sigint(int)
{
  if (global_exit_xboxdrv)
  {
    if (!g_options->quiet)
      std::cout << "Ctrl-c pressed twice, exiting hard" << std::endl;
    exit(EXIT_SUCCESS);
  }
  else
  {
    if (!g_options->quiet)
      std::cout << "Shutdown initiated, press Ctrl-c again if nothing is happening" << std::endl;

    global_exit_xboxdrv = true; 
  }
}

void on_sigterm(int)
{
  if (!g_options->quiet)
    std::cout << "Shutdown initiated by SIGTERM" << std::endl;

  exit(EXIT_SUCCESS);
}

void
Xboxdrv::run_list_controller()
{
  int ret = libusb_init(NULL);
  if (ret != LIBUSB_SUCCESS)
  {
    raise_exception(std::runtime_error, "libusb_init() failed: " << usb_strerror(ret));
  }

  libusb_device** list;
  ssize_t num_devices = libusb_get_device_list(NULL, &list);

  int id = 0;
  std::cout << " id | wid | idVendor | idProduct | Name" << std::endl;
  std::cout << "----+-----+----------+-----------+--------------------------------------" << std::endl;

  for(ssize_t dev_it = 0; dev_it < num_devices; ++dev_it)
  {
    libusb_device* dev = list[dev_it];
    libusb_device_descriptor desc;

    // FIXME: we silently ignore failures
    if (libusb_get_device_descriptor(dev, &desc) == LIBUSB_SUCCESS)
    {
      for(int i = 0; i < xpad_devices_count; ++i)
      {
        if (desc.idVendor  == xpad_devices[i].idVendor &&
            desc.idProduct == xpad_devices[i].idProduct)
        {
          if (xpad_devices[i].type == GAMEPAD_XBOX360_WIRELESS)
          {
            for(int wid = 0; wid < 4; ++wid)
            {
              std::cout << boost::format(" %2d |  %2d |   0x%04x |    0x%04x | %s (Port: %s)")
                % id
                % wid
                % int(xpad_devices[i].idVendor)
                % int(xpad_devices[i].idProduct)
                % xpad_devices[i].name 
                % wid
                        << std::endl;
            }
          }
          else
          {
            std::cout << boost::format(" %2d |  %2d |   0x%04x |    0x%04x | %s")
              % id
              % 0
              % int(xpad_devices[i].idVendor)
              % int(xpad_devices[i].idProduct)
              % xpad_devices[i].name 
                      << std::endl;
          }
          id += 1;
          break;
        }
      }
    }
  }

  if (id == 0)
    std::cout << "\nNo controller detected" << std::endl; 

  libusb_free_device_list(list, 1 /* unref_devices */);
}

bool
Xboxdrv::find_controller_by_path(const std::string& busid_str, const std::string& devid_str, 
                                 libusb_device** xbox_device) const
{
  int busid = boost::lexical_cast<int>(busid_str);
  int devid = boost::lexical_cast<int>(devid_str);

  libusb_device** list;
  ssize_t num_devices = libusb_get_device_list(NULL, &list);

  for(ssize_t dev_it = 0; dev_it < num_devices; ++dev_it)
  {
    libusb_device* dev = list[dev_it];

    if (libusb_get_bus_number(dev)     == busid &&
        libusb_get_device_address(dev) == devid)
    {
      *xbox_device = dev;

      // incrementing ref count, user must call unref
      libusb_ref_device(*xbox_device);
      libusb_free_device_list(list, 1 /* unref_devices */);
      return true;
    }
  }

  libusb_free_device_list(list, 1 /* unref_devices */);
  return false;
}

/** find the number of the next unused /dev/input/jsX device */
int
Xboxdrv::find_jsdev_number() const
{
  for(int i = 0; ; ++i)
  {
    char filename1[32];
    char filename2[32];

    sprintf(filename1, "/dev/input/js%d", i);
    sprintf(filename2, "/dev/js%d", i);

    if (access(filename1, F_OK) != 0 && access(filename2, F_OK) != 0)
      return i;
  }
}

/** find the number of the next unused /dev/input/eventX device */
int
Xboxdrv::find_evdev_number() const
{
  for(int i = 0; ; ++i)
  {
    char filename[32];

    sprintf(filename, "/dev/input/event%d", i);

    if (access(filename, F_OK) != 0)
      return i;
  }
}

bool
Xboxdrv::find_controller_by_id(int id, int vendor_id, int product_id, libusb_device** xbox_device) const
{
  libusb_device** list;
  ssize_t num_devices = libusb_get_device_list(NULL, &list);

  int id_count = 0;
  for(ssize_t dev_it = 0; dev_it < num_devices; ++dev_it)
  {
    libusb_device* dev = list[dev_it];
    libusb_device_descriptor desc;

    int ret = libusb_get_device_descriptor(dev, &desc);
    if (ret != LIBUSB_SUCCESS)
    {
      log_warn("libusb_get_device_descriptor() failed: " << usb_strerror(ret));
    }
    else
    {
      if (desc.idVendor  == vendor_id &&
          desc.idProduct == product_id)
      {
        if (id_count == id)
        {
          *xbox_device = dev;
          // increment ref count, user must free the device
          libusb_ref_device(*xbox_device);
          libusb_free_device_list(list, 1 /* unref_devices */);
          return true;
        }
        else
        {
          id_count += 1;
        }
      }
    }
  }

  libusb_free_device_list(list, 1 /* unref_devices */);
  return false;
}

bool
Xboxdrv::find_xbox360_controller(int id, libusb_device** xbox_device, XPadDevice* type) const
{
  libusb_device** list;
  ssize_t num_devices = libusb_get_device_list(NULL, &list);

  int id_count = 0;
  for(ssize_t dev_it = 0; dev_it < num_devices; ++dev_it)
  {
    libusb_device* dev = list[dev_it];
    libusb_device_descriptor desc;

    int ret = libusb_get_device_descriptor(dev, &desc);
    if (ret != LIBUSB_SUCCESS)
    {
      log_warn("libusb_get_device_descriptor() failed: " << usb_strerror(ret));
    }
    else
    {
      for(int i = 0; i < xpad_devices_count; ++i)
      {
        if (desc.idVendor  == xpad_devices[i].idVendor &&
            desc.idProduct == xpad_devices[i].idProduct)
        {
          if (id_count == id)
          {
            *xbox_device = dev;
            *type        = xpad_devices[i];
            // increment ref count, user must free the device
            libusb_ref_device(*xbox_device);
            libusb_free_device_list(list, 1 /* unref_devices */);
            return true;
          }
          else
          {
            id_count += 1;
          }
        }
      }
    }
  }

  libusb_free_device_list(list, 1 /* unref_devices */);
  return false;
}

void
Xboxdrv::find_controller(libusb_device** dev,
                         XPadDevice& dev_type,
                         const Options& opts) const
{
  if (opts.busid[0] != '\0' && opts.devid[0] != '\0')
  {
    if (opts.gamepad_type == GAMEPAD_UNKNOWN)
    {
      throw std::runtime_error("--device-by-path BUS:DEV option must be used in combination with --type TYPE option");
    }
    else
    {
      if (!find_controller_by_path(opts.busid, opts.devid, dev))
      {
        std::ostringstream out;
        out << "couldn't find device " << opts.busid << ":" << opts.devid;
        throw std::runtime_error(out.str());
      }
      else
      {
        dev_type.type      = opts.gamepad_type;
        dev_type.name      = "unknown";
        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(*dev, &desc) == LIBUSB_SUCCESS)
        {
          dev_type.idVendor  = desc.idVendor;
          dev_type.idProduct = desc.idProduct;
        }
      }
    }
  }
  else if (opts.vendor_id != -1 && opts.product_id != -1)
  {
    if (opts.gamepad_type == GAMEPAD_UNKNOWN)
    {
      throw std::runtime_error("--device-by-id VENDOR:PRODUCT option must be used in combination with --type TYPE option");
    }
    else 
    {
      if (!find_controller_by_id(opts.controller_id, opts.vendor_id, opts.product_id, dev))
      {
        std::ostringstream out;
        out << "Error: couldn't find device with " 
            << (boost::format("%04x:%04x") % opts.vendor_id % opts.product_id);
        throw std::runtime_error(out.str());
      }
      else
      {
        dev_type.type = opts.gamepad_type;
        dev_type.idVendor  = opts.vendor_id;
        dev_type.idProduct = opts.product_id;
        dev_type.name = "unknown";
      }
    }
  }
  else
  {
    if (!find_xbox360_controller(opts.controller_id, dev, &dev_type))
    {
      throw std::runtime_error("No Xbox or Xbox360 controller found");
    }
  }
}

void
Xboxdrv::print_copyright() const
{
  WordWrap wrap(get_terminal_width());
  wrap.para("xboxdrv " PACKAGE_VERSION " - http://pingus.seul.org/~grumbel/xboxdrv/");
  wrap.para("Copyright © 2008-2011 Ingo Ruhnke <grumbel@gmx.de>");
  wrap.para("Licensed under GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>");
  wrap.para("This program comes with ABSOLUTELY NO WARRANTY.");
  wrap.para("This is free software, and you are welcome to redistribute it under certain "
               "conditions; see the file COPYING for details.");
  wrap.newline();
}

void
Xboxdrv::run_main(const Options& opts)
{
  if (!opts.quiet)
  {
    print_copyright();
  }

  std::auto_ptr<XboxGenericController> controller;

  XPadDevice dev_type;

  if (!opts.evdev_device.empty())
  { // normal PC joystick via evdev
    controller = std::auto_ptr<XboxGenericController>(new EvdevController(opts.evdev_device, 
                                                                          opts.evdev_absmap, 
                                                                          opts.evdev_keymap,
                                                                          opts.evdev_grab,
                                                                          opts.evdev_debug));

    // FIXME: ugly, should be part of XboxGenericController
    dev_type.type = GAMEPAD_XBOX360;
    dev_type.idVendor  = 0;
    dev_type.idProduct = 0;
    dev_type.name = "Evdev device";
  }
  else
  { // regular USB Xbox360 controller    
    int ret = libusb_init(NULL);
    if (ret != LIBUSB_SUCCESS)
    {
      raise_exception(std::runtime_error, "libusb_init() failed: " << usb_strerror(ret));
    }
    
    // FIXME: this must be libusb_unref_device()'ed, child code must not keep a copy around
    libusb_device* dev = 0;
  
    find_controller(&dev, dev_type, opts);

    if (!dev)
    {
      throw std::runtime_error("No suitable USB device found, abort");
    }
    else 
    {
      if (!opts.quiet)
        print_info(dev, dev_type, opts);

      controller = XboxControllerFactory::create(dev_type, dev, opts);
    }
  }

  global_controller = controller.get();

  int jsdev_number = find_jsdev_number();
  int evdev_number = find_evdev_number();

  if (opts.led == -1)
    controller->set_led(2 + jsdev_number % 4);
  else
    controller->set_led(opts.led);

  if (opts.rumble_l != -1 && opts.rumble_r != -1)
  { // Only set rumble when explicitly requested
    controller->set_rumble(opts.rumble_l, opts.rumble_r);
  }

  if (!opts.quiet)
    std::cout << std::endl;
      
  if (opts.instant_exit)
  {
    usleep(1000);
  }
  else
  {          
    std::auto_ptr<UInput> uinput;
    if (!opts.no_uinput)
    {
      if (!opts.quiet)
        std::cout << "Starting with uinput" << std::endl;
      uinput = std::auto_ptr<UInput>(new UInput());
    }
    else
    {
      if (!opts.quiet)
        std::cout << "Starting without uinput" << std::endl;
    }

    if (!opts.quiet)
    {
      std::cout << "\nYour Xbox/Xbox360 controller should now be available as:" << std::endl
                << "  /dev/input/js" << jsdev_number << std::endl
                << "  /dev/input/event" << evdev_number << std::endl;

      if (opts.silent)
      {          
        std::cout << "\nPress Ctrl-c to quit\n" << std::endl;
      }
      else
      {
        std::cout << "\nPress Ctrl-c to quit, use '--silent' to suppress the event output\n" << std::endl;
      }
    }

    global_exit_xboxdrv = false;

    std::auto_ptr<MessageProcessor> message_proc;
    if (uinput.get())
    {
      ControllerSlotConfigPtr config_set = ControllerSlotConfig::create(*uinput, 
                                                                        0, opts.extra_devices,
                                                                        opts.get_controller_slot());

      // After all the ControllerConfig registered their events, finish up
      // the device creation
      uinput->finish();

      message_proc.reset(new UInputMessageProcessor(*uinput, config_set, opts));
    }
    else
    {
      message_proc.reset(new DummyMessageProcessor);
    }

    XboxdrvThread loop(message_proc, controller, opts);
    loop.controller_loop(opts);
          
    if (!opts.quiet) 
      std::cout << "Shutdown complete" << std::endl;
  }
}

void
Xboxdrv::print_info(libusb_device* dev,
                    const XPadDevice& dev_type,
                    const Options& opts) const
{
  libusb_device_descriptor desc;
  int ret = libusb_get_device_descriptor(dev, &desc);
  if (ret != LIBUSB_SUCCESS)
  {
    raise_exception(std::runtime_error, "libusb_get_device_descriptor() failed: " << usb_strerror(ret));
  }

  std::cout << "USB Device:        " << boost::format("%03d:%03d")
    % static_cast<int>(libusb_get_bus_number(dev))
    % static_cast<int>(libusb_get_device_address(dev)) << std::endl;
  std::cout << "Controller:        " << dev_type.name << std::endl;
  std::cout << "Vendor/Product:    " << boost::format("%04x:%04x")
    % uint16_t(desc.idVendor) % uint16_t(desc.idProduct) << std::endl;
  if (dev_type.type == GAMEPAD_XBOX360_WIRELESS)
    std::cout << "Wireless Port:     " << opts.wireless_id << std::endl;
  else
    std::cout << "Wireless Port:     -" << std::endl;
  std::cout << "Controller Type:   " << dev_type.type << std::endl;

  //std::cout << "ForceFeedback:     " << ((opts.controller.back().uinput.force_feedback) ? "enabled" : "disabled") << std::endl;
}

void
Xboxdrv::run_list_supported_devices()
{
  for(int i = 0; i < xpad_devices_count; ++i)
  {
    std::cout << boost::format("%s 0x%04x 0x%04x %s\n")
      % gamepadtype_to_string(xpad_devices[i].type)
      % int(xpad_devices[i].idVendor)
      % int(xpad_devices[i].idProduct)
      % xpad_devices[i].name;
  }    
}

bool xpad_device_sorter(const XPadDevice& lhs, const XPadDevice& rhs)
{
  if (lhs.idVendor < rhs.idVendor)
  {
    return true;
  }
  else if (lhs.idVendor == rhs.idVendor)
  {
    return lhs.idProduct < rhs.idProduct;
  }
  else
  {
    return false;
  }
}

void
Xboxdrv::run_list_supported_devices_xpad()
{
  boost::scoped_array<XPadDevice> sorted_devices(new XPadDevice[xpad_devices_count]);
  memcpy(sorted_devices.get(), xpad_devices, sizeof(XPadDevice) * xpad_devices_count);

  std::sort(sorted_devices.get(), sorted_devices.get() + xpad_devices_count, xpad_device_sorter);

  for(int i = 0; i < xpad_devices_count; ++i)
  {
    std::cout << boost::format("{ 0x%04x, 0x%04x, \"%s\", %s },\n")
      % int(sorted_devices[i].idVendor)
      % int(sorted_devices[i].idProduct)
      % sorted_devices[i].name
      % gamepadtype_to_macro_string(sorted_devices[i].type);
  }    
}

void
Xboxdrv::run_help_devices()
{
  std::cout << " idVendor | idProduct | Name" << std::endl;
  std::cout << "----------+-----------+---------------------------------" << std::endl;
  for(int i = 0; i < xpad_devices_count; ++i)
  {
    std::cout << boost::format("   0x%04x |    0x%04x | %s")
      % int(xpad_devices[i].idVendor)
      % int(xpad_devices[i].idProduct)
      % xpad_devices[i].name 
              << std::endl;
  }           
}

void
Xboxdrv::run_daemon(const Options& opts)
{
  if (!opts.quiet)
  {
    print_copyright();
  }

  int ret = libusb_init(NULL);
  if (ret != LIBUSB_SUCCESS)
  {
    raise_exception(std::runtime_error, "libusb_init() failed: " << usb_strerror(ret));
  }

  if (!opts.detach)
  {
    XboxdrvDaemon daemon;
    daemon.run(opts);
  }
  else
  {
    pid_t pid = fork();

    if (pid < 0) 
    { // fork error
      std::ostringstream out;
      out << "Xboxdrv::run_daemon(): failed to fork(): " << strerror(errno);
      throw std::runtime_error(out.str());
    }
    else if (pid > 0) 
    { // parent, just exit
      _exit(EXIT_SUCCESS);
    }
    else
    { // child, run daemon
      pid_t sid = setsid();

      if (sid == static_cast<pid_t>(-1))
      {
        std::ostringstream out;
        out << "Xboxdrv::run_daemon(): failed to setsid(): " << strerror(errno);
        throw std::runtime_error(out.str());
      }
      else
      {
        if (chdir("/") != 0)
        {
          std::ostringstream out;
          out << "Xboxdrv::run_daemon(): failed to chdir(\"/\"): " << strerror(errno);
          throw std::runtime_error(out.str());
        }
        else
        {
          XboxdrvDaemon daemon;
          daemon.run(opts);
        }
      }
    }
  }

  libusb_exit(NULL);
}

void
Xboxdrv::run_list_enums(uint32_t enums)
{
  const int terminal_width = get_terminal_width();

  WordWrap wrap(terminal_width);

  if (enums & Options::LIST_ABS)
  {
    wrap.println("EV_ABS:");
    wrap.para("  ", boost::algorithm::join(evdev_abs_names.get_names(), ", "));
    wrap.newline();
  }
  
  if (enums & Options::LIST_REL)
  {
    wrap.println("EV_REL:");
    wrap.para("  ", boost::algorithm::join(evdev_rel_names.get_names(), ", "));
    wrap.newline();
  }
  
  if (enums & Options::LIST_KEY)
  {
    wrap.println("EV_KEY:");
    wrap.para("  ", boost::algorithm::join(evdev_key_names.get_names(), ", "));
    wrap.newline();
  }
  
  if (enums & Options::LIST_X11KEYSYM)
  {
    std::vector<std::string> lst;  
    for(X11KeysymEnum::const_iterator i = get_x11keysym_names().begin();
        i != get_x11keysym_names().end(); ++i)
    {
      lst.push_back(i->second);
    }
    wrap.println("X11Keysym:");
    wrap.para("  ", boost::algorithm::join(lst, ", "));
    wrap.newline();
  }
  
  if (enums & Options::LIST_AXIS)
  {
    std::vector<std::string> lst;
    for(int i = 1; i < XBOX_AXIS_MAX; ++i)
    {
      lst.push_back(axis2string(static_cast<XboxAxis>(i)));
    }
    wrap.println("XboxAxis:");
    wrap.para("  ", boost::algorithm::join(lst, ", "));
    wrap.newline();
  }
  
  if (enums & Options::LIST_BUTTON)
  {
    std::vector<std::string> lst;
    for(int i = 1; i < XBOX_BTN_MAX; ++i)
    {
      lst.push_back(btn2string(static_cast<XboxButton>(i)));
    }
    wrap.println("XboxButton:");
    wrap.para("  ", boost::algorithm::join(lst, ", "));
    wrap.newline();
  }
}

Xboxdrv::Xboxdrv()
{
}

Xboxdrv::~Xboxdrv()
{
}

int
Xboxdrv::main(int argc, char** argv)
{
  try 
  {
    signal(SIGINT,  on_sigint);
    signal(SIGTERM, on_sigterm);

    Options opts;
    g_options = &opts;

    CommandLineParser cmd_parser;
    cmd_parser.parse_args(argc, argv, &opts);

    switch(opts.mode)
    {
      case Options::PRINT_HELP_DEVICES:
        run_help_devices();
        break;

      case Options::RUN_LIST_SUPPORTED_DEVICES:
        run_list_supported_devices();
        break;

      case Options::RUN_LIST_SUPPORTED_DEVICES_XPAD:
        run_list_supported_devices_xpad();
        break;

      case Options::PRINT_VERSION:
        cmd_parser.print_version();
        break;

      case Options::PRINT_HELP:
        cmd_parser.print_help();
        break;

      case Options::PRINT_LED_HELP:
        cmd_parser.print_led_help();
        break;

      case Options::RUN_DEFAULT:
        run_main(opts);
        break;

      case Options::PRINT_ENUMS:
        run_list_enums(opts.list_enums);
        break;

      case Options::RUN_DAEMON:
        run_daemon(opts);
        break;

      case Options::RUN_LIST_CONTROLLER:
        run_list_controller();
        break;
    }
  }
  catch(const std::exception& err)
  {
    std::cout << "\n-- [ ERROR ] ------------------------------------------------------\n"
              << err.what() << std::endl;
  }

  return 0;
}

int main(int argc, char** argv)
{
  Xboxdrv xboxdrv;
  return xboxdrv.main(argc, argv);
}

/* EOF */

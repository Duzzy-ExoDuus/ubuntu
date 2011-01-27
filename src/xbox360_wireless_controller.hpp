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

#ifndef HEADER_XBOX360_WIRELESS_CONTROLLER_HPP
#define HEADER_XBOX360_WIRELESS_CONTROLLER_HPP

#include <libusb.h>
#include <string>

#include "xbox_generic_controller.hpp"

struct XboxGenericMsg;
struct XPadDevice;

/** */
class Xbox360WirelessController : public XboxGenericController
{
private:
  libusb_device* dev;
  libusb_device_handle* handle;
  int endpoint;
  int interface;
  int battery_status;
  std::string serial;
  int led_status;

public:
  Xbox360WirelessController(libusb_device* dev, int controller_id, bool try_detach);
  virtual ~Xbox360WirelessController();

  void set_rumble(uint8_t left, uint8_t right);
  void set_led(uint8_t status);
  bool read(XboxGenericMsg& msg, int timeout);
  uint8_t get_battery_status() const;
private:
  Xbox360WirelessController (const Xbox360WirelessController&);
  Xbox360WirelessController& operator= (const Xbox360WirelessController&);
};

#endif

/* EOF */

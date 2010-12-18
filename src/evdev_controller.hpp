/*
**  Xbox360 USB Gamepad Userspace Driver
**  Copyright (C) 2010 Ingo Ruhnke <grumbel@gmx.de>
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

#ifndef HEADER_XBOXDRV_EVDEV_CONTROLLER_HPP
#define HEADER_XBOXDRV_EVDEV_CONTROLLER_HPP

#include <linux/input.h>
#include <string>
#include <map>
#include <queue>

#include "xboxmsg.hpp"
#include "evdev_absmap.hpp"
#include "xbox_generic_controller.hpp"

class EvdevAbsMap;

class EvdevController : public XboxGenericController
{
private:
  int m_fd;
  std::string m_name;

  EvdevAbsMap m_absmap;

  typedef std::map<int, XboxButton> KeyMap;
  KeyMap m_keymap;

  std::vector<struct input_absinfo> m_absinfo;
  typedef std::queue<struct input_event> EventBuffer;
  EventBuffer m_event_buffer;

  XboxGenericMsg m_msg;

public:
  EvdevController(const std::string& filename, 
                  const EvdevAbsMap&  absmap,
                  const std::map<int, XboxButton>& keyMap);

  void set_rumble(uint8_t left, uint8_t right);
  void set_led(uint8_t status);

  /** @param timeout   timeout in msec, 0 means forever */
  bool read(XboxGenericMsg& msg, bool verbose, int timeout);

private:
  bool apply(XboxGenericMsg& msg, const struct input_event& ev);
  void read_data_to_buffer();

private:
  EvdevController(const EvdevController&);
  EvdevController& operator=(const EvdevController&);
};

#endif

/* EOF */

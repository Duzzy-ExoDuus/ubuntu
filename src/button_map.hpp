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

#ifndef HEADER_XBOXDRV_BUTTON_MAP_HPP
#define HEADER_XBOXDRV_BUTTON_MAP_HPP

#include <assert.h>

#include "button_event.hpp"
#include "xboxmsg.hpp"

class ButtonMap
{
private:
  ButtonEvent btn_map[XBOX_BTN_MAX][XBOX_BTN_MAX];
  
public:
  ButtonMap();

  void bind(XboxButton code, const ButtonEvent& event);
  void bind(XboxButton shift_code, XboxButton code, const ButtonEvent& event);

  ButtonEvent lookup(XboxButton code) const;
  ButtonEvent lookup(XboxButton shift_code, XboxButton code) const;

  void clear();
};

#endif

/* EOF */

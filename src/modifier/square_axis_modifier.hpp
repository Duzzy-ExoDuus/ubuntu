/*
**  Xbox360 USB Gamepad Userspace Driver
**  Copyright (C) 2011 Ingo Ruhnke <grumbel@gmx.de>
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

#ifndef HEADER_XBOXDRV_MODIFIER_SQUARE_AXIS_MODIFIER_HPP
#define HEADER_XBOXDRV_MODIFIER_SQUARE_AXIS_MODIFIER_HPP

#include "modifier.hpp"

class SquareAxisModifier : public Modifier
{
private:
public:
  SquareAxisModifier();

  void update(int msec_delta, XboxGenericMsg& msg);

private:
  SquareAxisModifier(const SquareAxisModifier&);
  SquareAxisModifier& operator=(const SquareAxisModifier&);
};

#endif

/* EOF */

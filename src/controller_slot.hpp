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

#ifndef HEADER_XBOXDRV_CONTROLLER_SLOT_HPP
#define HEADER_XBOXDRV_CONTROLLER_SLOT_HPP

#include <vector>

#include "controller_slot_config.hpp"

class XboxdrvThread;

class ControllerSlot
{
private:
  int m_id;
  ControllerSlotConfigPtr m_config;
  std::vector<ControllerMatchRulePtr> m_rules;
  int m_led_status;
  XboxdrvThread* m_thread;

  uint8_t m_busnum;
  uint8_t m_devnum;
  XPadDevice m_dev_type;
    
public:
  ControllerSlot() :
    m_id(),
    m_config(),
    m_rules(),
    m_led_status(-1),
    m_thread(0),
    m_busnum(),
    m_devnum(),
    m_dev_type()
  {}

  ControllerSlot(int id_,
                 ControllerSlotConfigPtr config_,
                 std::vector<ControllerMatchRulePtr> rules_,
                 int led_status_,
                 XboxdrvThread* thread_ = 0) :
    m_id(id_),
    m_config(config_),
    m_rules(rules_),
    m_led_status(led_status_),
    m_thread(thread_),
    m_busnum(),
    m_devnum(),
    m_dev_type()
  {}

  ControllerSlot(const ControllerSlot& rhs) :
    m_id(rhs.m_id),
    m_config(rhs.m_config),
    m_rules(rhs.m_rules),
    m_led_status(rhs.m_led_status),
    m_thread(rhs.m_thread),
    m_busnum(rhs.m_busnum),
    m_devnum(rhs.m_devnum),
    m_dev_type(rhs.m_dev_type)
  {}

  ControllerSlot& operator=(const ControllerSlot& rhs)
  {
    if (&rhs != this)
    {
      m_id     = rhs.m_id;
      m_config = rhs.m_config;
      m_rules  = rhs.m_rules;
      m_led_status = rhs.m_led_status;
      m_thread = rhs.m_thread;
      m_busnum = rhs.m_busnum;
      m_devnum = rhs.m_devnum;
      m_dev_type = rhs.m_dev_type;
    }
    return *this;
  }

  bool is_connected() const;
  void connect(XboxdrvThread* thread, 
               uint8_t busnum, uint8_t devnum,
               const XPadDevice& dev_type);
  void disconnect();
  bool try_disconnect();

  const std::vector<ControllerMatchRulePtr>& get_rules() const { return m_rules; }
  int get_led_status() const { return m_led_status; }
  int get_id() const { return m_id; }
  ControllerSlotConfigPtr get_config() const { return m_config; }

  std::string get_usbpath() const;
  std::string get_usbid() const;
  std::string get_name() const;
};

#endif

/* EOF */

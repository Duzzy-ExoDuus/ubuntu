/* 
**  Xbox360 USB Gamepad Userspace Driver
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

#ifndef HEADER_UINPUT_HPP
#define HEADER_UINPUT_HPP

#include <boost/thread/mutex.hpp>

#include "axis_event.hpp"
#include "linux_uinput.hpp"

struct Xbox360Msg;
struct XboxMsg;
struct Xbox360GuitarMsg;
  
class UInput
{
private:
  typedef std::map<uint32_t, boost::shared_ptr<LinuxUinput> > UInputDevs;
  UInputDevs uinput_devs;

  struct RelRepeat 
  {
    UIEvent code;
    int value;
    int time_count;
    int repeat_interval;
  };

  std::map<UIEvent, RelRepeat> rel_repeat_lst;

  boost::mutex m_mutex;

public:
  UInput();
  ~UInput();

  void update(int msec_delta);

  void set_ff_callback(int device_id, const boost::function<void (uint8_t, uint8_t)>& callback);

  /** Device construction functions
      @{*/
  void add_rel(uint32_t device_id, int ev_code);
  void add_abs(uint32_t device_id, int ev_code, int min, int max, int fuzz, int flat);
  void add_key(uint32_t device_id, int ev_code);
  void add_ff(uint32_t device_id, uint16_t code);

  /** needs to be called to finish device creation and create the
      device in the kernel */
  void finish();
  /** @} */

  /** Send events to the kernel
      @{*/
  void send(uint32_t device_id, int ev_type, int ev_code, int value);
  void send_abs(uint32_t device_id, int ev_code, int value);
  void send_key(uint32_t device_id, int ev_code, bool value);
  void send_rel_repetitive(const UIEvent& code, int value, int repeat_interval);

  /** should be called to single that all events of the current frame
      have been send */
  void sync();
  /** @} */

  boost::mutex& get_mutex() { return m_mutex; }

private:
  /** create a LinuxUinput with the given device_id, if some already
      exist return a pointer to it */
  LinuxUinput* create_uinput_device(uint32_t device_id);

  /** must only be called with a valid device_id */
  LinuxUinput* get_uinput(uint32_t device_id) const;
};

#endif

/* EOF */

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

#include "uinput.hpp"

#include "log.hpp"

UInput::UInput() :
  uinput_devs(),
  rel_repeat_lst(),
  m_mutex()
{
}

LinuxUinput*
UInput::create_uinput_device(uint32_t device_id)
{ 
  // DEVICEID_AUTO should not happen at this point as the user should
  // have called resolve_device_id()
  assert(device_id != DEVICEID_AUTO);

  UInputDevs::iterator it = uinput_devs.find(device_id);
  if (it != uinput_devs.end())
  {
    // device already exist, so return it
    return it->second.get();
  }
  else
  {
    log_debug("create device: " << device_id);
    LinuxUinput::DeviceType device_type = LinuxUinput::kGenericDevice;
    std::ostringstream dev_name;
    dev_name << "Xbox Gamepad (userspace driver)";

    switch (device_id)
    {
      case DEVICEID_JOYSTICK:
        device_type = LinuxUinput::kJoystickDevice;
        break;

      case DEVICEID_MOUSE:
        device_type = LinuxUinput::kMouseDevice;
        dev_name << " - Mouse Emulation";
        break;
      
      case DEVICEID_KEYBOARD:
        device_type = LinuxUinput::kGenericDevice;
        dev_name << " - Keyboard Emulation";
        break;

      default:
        dev_name << " - " << device_id+1;
        break;
    }

    boost::shared_ptr<LinuxUinput> dev(new LinuxUinput(device_type, dev_name.str(), 0x0000, 0x0000));
    uinput_devs.insert(std::pair<int, boost::shared_ptr<LinuxUinput> >(device_id, dev));

    log_debug("created uinput device: " << device_id << " - '" << dev_name.str() << "'");

    return dev.get();
  }
}

UInput::~UInput()
{
}

void
UInput::add_key(uint32_t device_id, int ev_code)
{
  LinuxUinput* dev = create_uinput_device(device_id);
  dev->add_key(ev_code);
}

void
UInput::add_rel(uint32_t device_id, int ev_code)
{
  LinuxUinput* dev = create_uinput_device(device_id);
  dev->add_rel(ev_code);
}

void
UInput::add_abs(uint32_t device_id, int ev_code, int min, int max, int fuzz, int flat)
{
  LinuxUinput* dev = create_uinput_device(device_id);
  dev->add_abs(ev_code, min, max, fuzz, flat);
}

void
UInput::add_ff(uint32_t device_id, uint16_t code)
{
  LinuxUinput* dev = create_uinput_device(device_id);
  dev->add_ff(code);
}

void
UInput::finish()
{
  for(UInputDevs::iterator i = uinput_devs.begin(); i != uinput_devs.end(); ++i)
  {
    i->second->finish();
  }
}

void
UInput::send(uint32_t device_id, int ev_type, int ev_code, int value)
{
  get_uinput(device_id)->send(ev_type, ev_code, value);
}

void
UInput::send_abs(uint32_t device_id, int ev_code, int value)
{
  assert(ev_code != -1);
  get_uinput(device_id)->send(EV_ABS, ev_code, value);
}

void
UInput::send_key(uint32_t device_id, int ev_code, bool value)
{
  assert(ev_code != -1);

  get_uinput(device_id)->send(EV_KEY, ev_code, value);
}

void
UInput::update(int msec_delta)
{
  for(std::map<UIEvent, RelRepeat>::iterator i = rel_repeat_lst.begin(); i != rel_repeat_lst.end(); ++i)
  {
    i->second.time_count += msec_delta;

    while (i->second.time_count >= i->second.repeat_interval)
    {
      get_uinput(i->second.code.get_device_id())->send(EV_REL, i->second.code.code, i->second.value);
      i->second.time_count -= i->second.repeat_interval;
    }
  }

  for(UInputDevs::iterator i = uinput_devs.begin(); i != uinput_devs.end(); ++i)
  {
    i->second->update(msec_delta);
  }
}

void
UInput::sync()
{
  for(UInputDevs::iterator i = uinput_devs.begin(); i != uinput_devs.end(); ++i)
  {
    i->second->sync();
  }
}

void
UInput::send_rel_repetitive(const UIEvent& code, int value, int repeat_interval)
{
  if (repeat_interval < 0)
  { // remove rel_repeats from list
    rel_repeat_lst.erase(code);
    // no need to send a event for rel, as it defaults to 0 anyway
  }
  else
  { // add rel_repeats to list
    std::map<UIEvent, RelRepeat>::iterator it = rel_repeat_lst.find(code);

    if (it == rel_repeat_lst.end())
    {
      RelRepeat rel_rep;
      rel_rep.code  = code;
      rel_rep.value = value;
      rel_rep.time_count = 0;
      rel_rep.repeat_interval = repeat_interval;
      rel_repeat_lst.insert(std::pair<UIEvent, RelRepeat>(code, rel_rep));
    
      // Send the event once
      get_uinput(code.get_device_id())->send(EV_REL, code.code, value);
    }
    else
    {
      it->second.code  = code;
      it->second.value = value;
      // it->second.time_count = do not touch this
      it->second.repeat_interval = repeat_interval;
    }
  }
}

LinuxUinput*
UInput::get_uinput(uint32_t device_id) const
{
  UInputDevs::const_iterator it = uinput_devs.find(device_id);
  if (it != uinput_devs.end())
  {
    return it->second.get();
  }
  else
  {
    assert(0);
    std::ostringstream str;
    str << "Couldn't find uinput device: " << device_id;
    throw std::runtime_error(str.str());
  }
}

void
UInput::set_ff_callback(int device_id, const boost::function<void (uint8_t, uint8_t)>& callback)
{
  get_uinput(device_id)->set_ff_callback(callback);
}

/* EOF */

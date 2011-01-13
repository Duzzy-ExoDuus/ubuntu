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

#include "headset.hpp"

#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include <string.h>

Headset::Headset(struct usb_dev_handle* handle, 
                 bool debug,
                 const std::string& dump_filename,
                 const std::string& play_filename) :
  m_handle(handle),
  m_quit_read_thread(false),
  m_quit_write_thread(false)
{
  int ret = usb_claim_interface(m_handle, 1);

  if (ret < 0)
  {
    std::ostringstream out;
    out << "[headset] " << usb_strerror();
    throw std::runtime_error(out.str());
  }

  m_read_thread.reset(new boost::thread(boost::bind(&Headset::read_thread, this, dump_filename, debug)));

  if (!play_filename.empty())
  {
    m_write_thread.reset(new boost::thread(boost::bind(&Headset::write_thread, this, play_filename)));
  }
}

Headset::~Headset()
{
  if (m_read_thread.get())
    m_read_thread->join();

  if (m_write_thread.get())
    m_write_thread->join();

  m_read_thread.release();
  m_write_thread.release();

  int err = usb_release_interface(m_handle, 1);

  if (err < 0)
  {
    std::ostringstream out;
    out << "[headset] " << usb_strerror();
    throw std::runtime_error(out.str());
  }
}

void
Headset::write_thread(const std::string& filename)
{
  std::ifstream in(filename.c_str(), std::ios::binary);

  if (!in)
  {
    std::ostringstream out;
    out << "[headset] " << filename << ": " << strerror(errno);
    throw std::runtime_error(out.str());    
  }

  std::cout << "[headset] starting playback: " << filename << std::endl;

  char data[32];
  while(in)
  {
    int len = in.read(data, sizeof(data)).gcount();

    if (len != 32)
    {
      // ignore short reads
    }
    else
    {
      const int ret = usb_interrupt_write(m_handle, 4, reinterpret_cast<char*>(data), sizeof(data), 0);
      if (ret < 0)
      {
        std::ostringstream out;
        out << "[headset] " << usb_strerror();
        throw std::runtime_error(out.str());
      }
    }
  }

  std::cout << "[headset] finished playback: " << filename << std::endl;
}

void
Headset::read_thread(const std::string& filename, bool debug)
{
  std::auto_ptr<std::ofstream> out;
  
  if (!filename.empty())
  {
    out.reset(new std::ofstream(filename.c_str(), std::ios::binary));
  
    if (!*out)
    {
      std::ostringstream outstr;
      outstr << "[headset] " << filename << ": " << strerror(errno);
      throw std::runtime_error(outstr.str());
    }
  }

  while(!m_quit_read_thread)
  {
    uint8_t data[32];
    const int ret = usb_interrupt_read(m_handle, 3, reinterpret_cast<char*>(data), sizeof(data), 0);
    if (ret < 0)
    {
      std::ostringstream outstr;
      outstr << "[headset] " << usb_strerror();
      throw std::runtime_error(outstr.str());
    }
    else
    {
      if (ret == 0)
      {
        std::cout << "[headset] -- empty read --" << std::endl;
      }
      else
      {
        if (out.get())
        {
          out->write(reinterpret_cast<char*>(data), sizeof(data));
        }

        if (debug)
        {
          std::cout << "[headset] ";
          for(int i = 0; i < ret; ++i)
          {
            std::cout << boost::format("0x%02x ") % int(data[i]);
          }
          std::cout << std::endl;
        }
      }
    }
  }
}

/* EOF */

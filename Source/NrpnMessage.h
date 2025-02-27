#ifndef MIDI2LR_NRPNMESSAGE_H_INCLUDED
#define MIDI2LR_NRPNMESSAGE_H_INCLUDED
/*
==============================================================================

NrpnMessage.h

This file is part of MIDI2LR. Copyright 2015 by Rory Jaffe.

MIDI2LR is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

MIDI2LR is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
MIDI2LR.  If not, see <http://www.gnu.org/licenses/>.
==============================================================================
*/
#include <array>

class NrpnFilter {
   // This  assumes that all NRPN messages have 4 messages, though the NRPN standard allows omission
   // of the 4th message. If the 4th message is dropped, this class silently consumes the message
   // without emitting anything. Caller must handle all concurrency considerations.
 public:
   struct ProcessResult {
      bool is_nrpn{};
      bool is_ready{};
      short control{};
      short value{};
   };
   ProcessResult operator()(short channel, short control, short value);

 private:
   static constexpr int kChannels{16};
   std::array<int, kChannels> control_msb_{};
   std::array<int, kChannels> control_lsb_{};
   std::array<int, kChannels> value_msb_{};
   std::array<int, kChannels> value_lsb_{};
   std::array<int, kChannels> ready_flags_{};
   void Clear(int channel) noexcept
   {
      ready_flags_[channel] = 0;
      control_msb_[channel] = 0;
      control_lsb_[channel] = 0;
      value_msb_[channel] = 0;
      value_lsb_[channel] = 0;
   }
};

#endif
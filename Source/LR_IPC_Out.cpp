/*
  ==============================================================================

    LR_IPC_OUT.cpp

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
#include "LR_IPC_Out.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <string>
#include <unordered_map>
#include <utility>

#include <gsl/gsl>
#include "ControlsModel.h"
#include "MIDIReceiver.h"
#include "MIDISender.h"
#include "MidiUtilities.h"
#include "Misc.h"
#include "Profile.h"

using Clock = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;

namespace {
   constexpr auto kHost{"127.0.0.1"};
   constexpr auto kTerminate{"!!!@#$%^"};
   constexpr int kConnectTimer{1000};
   constexpr int kConnectTryTime{100};
   constexpr int kDelay{8}; // in between recurrent actions
   constexpr int kLrOutPort{58763};
   constexpr int kMinRecenterTimer{250}; // give controller enough of a refractory period before
                                         // resetting it
   constexpr int kRecenterTimer{std::max(kMinRecenterTimer,
       kDelay + kDelay / 2)}; // don't change, change kDelay and kMinRecenterTimer
} // namespace

LrIpcOut::LrIpcOut(ControlsModel& c_model, const Profile& profile,
    std::shared_ptr<MidiSender> midi_sender, MidiReceiver& midi_receiver) noexcept
    : profile_{profile}, controls_model_{c_model}, midi_sender_{std::move(midi_sender)}
{
   midi_receiver.AddCallback(this, &LrIpcOut::MidiCmdCallback);
}

#pragma warning(push)
#pragma warning(disable : 26447)
LrIpcOut::~LrIpcOut()
{
   try {
      if (const auto m = command_.clear_count_emplace(kTerminate))
         rsj::Log(juce::String(m) + " left in queue in LrIpcOut destructor");
      connect_timer_.Stop();
      juce::InterprocessConnection::disconnect();
   }
   catch (...) {
      rsj::LogAndAlertError("Exception in LrIpcOut destructor.");
      std::terminate();
   }
}
#pragma warning(pop)

void LrIpcOut::Start()
{
   try {
      connect_timer_.Start();
      send_out_future_ = std::async(std::launch::async, &LrIpcOut::SendOut, this);
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void LrIpcOut::MidiCmdCallback(rsj::MidiMessage mm)
{
   struct RepeatMessage {
      std::string cw;
      std::string ccw;
   };
   using namespace std::string_literals;
   try {
      const rsj::MidiMessageId message{mm};
      static const std::unordered_map<std::string, RepeatMessage> kCmdUpDown{
          {"ChangeBrushSize"s, {"BrushSizeLarger 1\n"s, "BrushSizeSmaller 1\n"s}},
          {"ChangeCurrentSlider"s, {"SliderIncrease 1\n"s, "SliderDecrease 1\n"s}},
          {"ChangeFeatherSize"s, {"BrushFeatherLarger 1\n"s, "BrushFeatherSmaller 1\n"s}},
          {"ChangeLastDevelopParameter"s,
              {"IncrementLastDevelopParameter 1\n"s, "DecrementLastDevelopParameter 1\n"s}},
          {"Key32Key31"s, {"Key32 1\n"s, "Key31 1\n"s}},
          {"Key34Key33"s, {"Key34 1\n"s, "Key33 1\n"s}},
          {"Key36Key35"s, {"Key36 1\n"s, "Key35 1\n"s}},
          {"Key38Key37"s, {"Key38 1\n"s, "Key37 1\n"s}},
          {"Key40Key39"s, {"Key40 1\n"s, "Key39 1\n"s}},
          {"NextPrev"s, {"Next 1\n"s, "Prev 1\n"s}},
          {"RedoUndo"s, {"Redo 1\n"s, "Undo 1\n"s}},
          {"SelectRightLeft"s, {"Select1Right 1\n"s, "Select1Left 1\n"s}},
          {"ZoomInOut"s, {"ZoomInSmallStep 1\n"s, "ZoomOutSmallStep 1\n"s}},
          {"ZoomOutIn"s, {"ZoomOutSmallStep 1\n"s, "ZoomInSmallStep 1\n"s}},
      };
      if (!profile_.MessageExistsInMap(message))
         return;
      const auto command_to_send = profile_.GetCommandForMessage(message);
      if (command_to_send == "PrevPro"s || command_to_send == "NextPro"s
          || command_to_send == "Unmapped"s)
         return; // handled by ProfileManager
      // if it is a repeated command, change command_to_send appropriately
      if (const auto a = kCmdUpDown.find(command_to_send); a != kCmdUpDown.end()) {
         static TimePoint nextresponse{};
         if (const auto now = Clock::now(); nextresponse < now) {
            nextresponse = now + std::chrono::milliseconds(kDelay);
            if (mm.message_type_byte == rsj::kPwFlag
                || (mm.message_type_byte == rsj::kCcFlag
                       && controls_model_.GetCcMethod(mm.channel, mm.number)
                              == rsj::CCmethod::kAbsolute)) {
               recenter_.SetMidiMessage(mm);
            }
            const auto change = controls_model_.MeasureChange(mm);
            if (change == 0)
               return;      // don't send any signal
            if (change > 0) // turned clockwise
               SendCommand(a->second.cw);
            else // turned counterclockwise
               SendCommand(a->second.ccw);
         }
      }
      else { // not repeated command
         const auto computed_value = controls_model_.ControllerToPlugin(mm);
         SendCommand(command_to_send + ' ' + std::to_string(computed_value) + '\n');
      }
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void LrIpcOut::SendCommand(std::string&& command)
{
   try {
      if (sending_stopped_)
         return;
      command_.push(std::move(command));
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void LrIpcOut::SendCommand(const std::string& command)
{
   try {
      if (sending_stopped_)
         return;
      command_.push(command);
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void LrIpcOut::Stop()
{
   try {
      sending_stopped_ = true;
      const auto connected = isConnected();
      for (const auto& cb : callbacks_)
         cb(connected, true);
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void LrIpcOut::Restart()
{
   try {
      using namespace std::string_literals;
      sending_stopped_ = false;
      const auto connected = isConnected();
      for (const auto& cb : callbacks_)
         cb(connected, false);
      // resync controls
      SendCommand("FullRefresh 1\n"s);
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void LrIpcOut::connectionMade()
{
   try {
      for (const auto& cb : callbacks_)
         cb(true, sending_stopped_);
      rsj::Log("Connected to Lightroom plugin");
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void LrIpcOut::connectionLost()
{
   try {
      for (const auto& cb : callbacks_)
         cb(false, sending_stopped_);
      rsj::Log("Disconnected from Lightroom plugin");
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void LrIpcOut::messageReceived(const juce::MemoryBlock& /*msg*/) noexcept {}

void LrIpcOut::SendOut()
{
   try {
      do {
         auto command_copy = command_.pop();
         if (command_copy == kTerminate)
            return;
         // check if there is a connection
         if (juce::InterprocessConnection::isConnected()) {
            if (command_copy.back() != '\n') // should be terminated with \n
               command_copy += '\n';
            juce::InterprocessConnection::getSocket()->write(
                command_copy.c_str(), gsl::narrow_cast<int>(command_copy.length()));
         }
      } while (true);
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void LrIpcOut::ConnectTimer::Start()
{
   try {
      auto lock = std::scoped_lock(connect_mutex_);
      juce::Timer::startTimer(kConnectTimer);
      timer_off_ = false;
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void LrIpcOut::ConnectTimer::Stop()
{
   try {
      auto lock = std::scoped_lock(connect_mutex_);
      juce::Timer::stopTimer();
      timer_off_ = true;
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void LrIpcOut::ConnectTimer::timerCallback()
{
   try {
      auto lock = std::scoped_lock(connect_mutex_);
      if (!timer_off_ && !owner_.juce::InterprocessConnection::isConnected())
         owner_.juce::InterprocessConnection::connectToSocket(kHost, kLrOutPort, kConnectTryTime);
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void LrIpcOut::Recenter::SetMidiMessage(rsj::MidiMessage mm)
{
   try {
      auto lock = std::scoped_lock(mtx_);
      mm_ = mm;
      juce::Timer::startTimer(kRecenterTimer);
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void LrIpcOut::Recenter::timerCallback()
{
   try {
      rsj::MidiMessage local_mm{};
      {
         auto lock = std::scoped_lock(mtx_);
         juce::Timer::stopTimer();
         local_mm = mm_;
      }
      const auto center = owner_.controls_model_.SetToCenter(local_mm);
      // send center to control//
      switch (local_mm.message_type_byte) {
      case rsj::kPwFlag: {
         owner_.midi_sender_->SendPitchWheel(local_mm.channel + 1, center);
         break;
      }
      case rsj::kCcFlag: {
         owner_.midi_sender_->SendCc(local_mm.channel + 1, local_mm.number, center);
         break;
      }
      default:
          /* no action */;
      }
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}
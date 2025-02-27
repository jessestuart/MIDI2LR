/*
  ==============================================================================

    Profile.cpp

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
#include "Profile.h"

#include <algorithm>

#include "Misc.h"

void Profile::AddCommandForMessageI(size_t command, const rsj::MidiMessageId& message)
{
   try {
      if (command < command_set_.CommandAbbrevSize()) {
         auto cmd_abbreviation = command_set_.CommandAbbrevAt(command);
         message_map_[message] = cmd_abbreviation;
         command_string_map_.emplace(cmd_abbreviation, message);
         SortI();
         profile_unsaved_ = true;
      }
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void Profile::AddRowMapped(const std::string& command, const rsj::MidiMessageId& message)
{
   try {
      auto guard = std::unique_lock{mutex_};
      if (!MessageExistsInMapI(message)) {
         if (!command_set_.CommandTextIndex(command)) {
            message_map_[message] = "Unmapped";
            command_string_map_.emplace("Unmapped", message);
         }
         else {
            message_map_[message] = command;
            command_string_map_.emplace(command, message);
         }
         command_table_.push_back(message);
         SortI();
         profile_unsaved_ = true;
      }
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void Profile::AddRowUnmapped(const rsj::MidiMessageId& message)
{
   try {
      auto guard = std::unique_lock{mutex_};
      if (!MessageExistsInMapI(message)) {
         AddCommandForMessageI(0, message); // add an entry for 'no command'
         command_table_.push_back(message);
         SortI();
         profile_unsaved_ = true;
      }
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void Profile::FromXml(const juce::XmlElement* root)
{ // external use only, but will either use external versions of Profile calls to lock individual
  // accesses or manually lock any internal calls instead of using mutex for entire method
   try {
      if (!root || root->getTagName().compare("settings") != 0)
         return;
      RemoveAllRows();
      const auto* setting = root->getFirstChildElement();
      while (setting) {
         if (setting->hasAttribute("controller")) {
            const rsj::MidiMessageId message{setting->getIntAttribute("channel"),
                setting->getIntAttribute("controller"), rsj::MsgIdEnum::kCc};
            AddRowMapped(setting->getStringAttribute("command_string").toStdString(), message);
         }
         else if (setting->hasAttribute("note")) {
            const rsj::MidiMessageId note{setting->getIntAttribute("channel"),
                setting->getIntAttribute("note"), rsj::MsgIdEnum::kNote};
            AddRowMapped(setting->getStringAttribute("command_string").toStdString(), note);
         }
         else if (setting->hasAttribute("pitchbend")) {
            const rsj::MidiMessageId pb{
                setting->getIntAttribute("channel"), 0, rsj::MsgIdEnum::kPitchBend};
            AddRowMapped(setting->getStringAttribute("command_string").toStdString(), pb);
         }
         setting = setting->getNextElement();
      }
      auto guard = std::unique_lock{mutex_};
      SortI();
      saved_map_ = message_map_;
      profile_unsaved_ = false;
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

std::vector<rsj::MidiMessageId> Profile::GetMessagesForCommand(const std::string& command) const
{
   try {
      auto guard = std::shared_lock{mutex_};
      std::vector<rsj::MidiMessageId> mm;
      const auto range = command_string_map_.equal_range(command);
      for (auto it = range.first; it != range.second; ++it)
         mm.push_back(it->second);
      return mm;
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void Profile::RemoveAllRows()
{
   try {
      auto guard = std::unique_lock{mutex_};
      command_string_map_.clear();
      command_table_.clear();
      message_map_.clear();
      profile_unsaved_ = false;
      // no reason for profile_unsaved_ here. nothing to save
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void Profile::RemoveMessage(const rsj::MidiMessageId& message)
{
   try {
      auto guard = std::unique_lock{mutex_};
      command_string_map_.erase(message_map_.at(message));
      message_map_.erase(message);
      profile_unsaved_ = true;
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void Profile::RemoveRow(size_t row)
{
   try {
      auto guard = std::unique_lock{mutex_};
      const auto msg = GetMessageForNumberI(row);
      command_string_map_.erase(message_map_.at(msg));
      command_table_.erase(command_table_.cbegin() + row);
      message_map_.erase(msg);
      profile_unsaved_ = true;
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void Profile::Resort(std::pair<int, bool> new_order)
{
   try {
      auto guard = std::unique_lock{mutex_};
      current_sort_ = new_order;
      SortI();
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void Profile::SortI()
{
   try {
      const auto msg_idx = [this](const rsj::MidiMessageId& a) {
         return command_set_.CommandTextIndex(GetCommandForMessageI(a));
      };
      const auto msg_sort = [&msg_idx](const rsj::MidiMessageId& a, const rsj::MidiMessageId& b) {
         return msg_idx(a) < msg_idx(b);
      };
      if (current_sort_.first == 1)
         if (current_sort_.second)
            std::sort(command_table_.begin(), command_table_.end());
         else
            std::sort(command_table_.rbegin(), command_table_.rend());
      else if (current_sort_.second)
         std::sort(command_table_.begin(), command_table_.end(), msg_sort);
      else
         std::sort(command_table_.rbegin(), command_table_.rend(), msg_sort);
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}

void Profile::ToXmlFile(const juce::File& file)
{
   try {
      auto guard = std::shared_lock{mutex_};
      if (!message_map_.empty()) { // don't bother if map is empty
         // save the contents of the command map to an xml file
         juce::XmlElement root{"settings"};
         for (const auto& map_entry : message_map_) {
            auto setting = std::make_unique<juce::XmlElement>("setting");
            setting->setAttribute("channel", map_entry.first.channel);
            switch (map_entry.first.msg_id_type) {
            case rsj::MsgIdEnum::kNote:
               setting->setAttribute("note", map_entry.first.data);
               break;
            case rsj::MsgIdEnum::kCc:
               setting->setAttribute("controller", map_entry.first.data);
               break;
            case rsj::MsgIdEnum::kPitchBend:
               setting->setAttribute("pitchbend", 0);
               break;
            }
            setting->setAttribute("command_string", map_entry.second);
            root.addChildElement(setting.release());
         }
         if (!root.writeToFile(file, "")) {
            // Give feedback if file-save doesn't work
            rsj::LogAndAlertError("Unable to save file as specified. Please try again, and "
                                  "consider saving to a different location. "
                                  + file.getFullPathName());
         }
         saved_map_ = message_map_;
         profile_unsaved_ = false;
      }
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse(typeid(this).name(), __func__, e);
      throw;
   }
}
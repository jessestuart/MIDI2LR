#ifndef MIDI2LR_SETTINGSMANAGER_H_INCLUDED
#define MIDI2LR_SETTINGSMANAGER_H_INCLUDED
/*
  ==============================================================================

    SettingsManager.h

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
#include <memory>

#include <JuceLibraryCode/JuceHeader.h>
class LrIpcOut;
class ProfileManager;

class SettingsManager final {
 public:
   SettingsManager(ProfileManager& profile_manager, std::weak_ptr<LrIpcOut>&& lr_ipc_out);
   ~SettingsManager() = default;
   SettingsManager(const SettingsManager& other) = delete;
   SettingsManager(SettingsManager&& other) = delete;
   SettingsManager& operator=(const SettingsManager& other) = delete;
   SettingsManager& operator=(SettingsManager&& other) = delete;
   [[nodiscard]] int GetAutoHideTime() const noexcept;
   [[nodiscard]] int GetLastVersionFound() const noexcept;
   [[nodiscard]] bool GetPickupEnabled() const noexcept;
   [[nodiscard]] juce::String GetProfileDirectory() const noexcept;
   void SetAutoHideTime(int new_time);
   void SetLastVersionFound(int version_number);
   void SetPickupEnabled(bool enabled);
   void SetProfileDirectory(const juce::String& profile_directory);

 private:
   ProfileManager& profile_manager_;
   std::unique_ptr<juce::PropertiesFile> properties_file_;
   std::weak_ptr<LrIpcOut> lr_ipc_out_;
   void ConnectionCallback(bool, bool);
};

#endif // SETTINGSMANAGER_H_INCLUDED

/*
==============================================================================

Translate.cpp

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
#include "Translate.h"

#include <exception>
#include <map>
#include <memory>

#include <JuceLibraryCode/JuceHeader.h>
#include "Misc.h"
#include "Translate.txt"

void rsj::Translate(const std::string& lg)
{
   try {
      static const std::map<std::string, const char*> kTranslationTable{{"de", de}, {"es", es},
          {"fr", fr}, {"it", it}, {"ja", ja}, {"ko", ko}, {"nl", nl}, {"pt", pt}, {"sv", sv},
          {"zh_cn", zh_cn}, {"zh_tw", zh_tw}};
      if (const auto found = kTranslationTable.find(lg); found != kTranslationTable.end()) {
         const juce::String str(juce::CharPointer_UTF8(found->second));
         auto ls = std::make_unique<juce::LocalisedStrings>(str, false);
         juce::LocalisedStrings::setCurrentMappings(ls.release()); // takes ownership of ls
      }
      else
         juce::LocalisedStrings::setCurrentMappings(nullptr);
   }
   catch (const std::exception& e) {
      rsj::ExceptionResponse("rsj", __func__, e);
      throw;
   }
}
/*
 This file is part of SpatGRIS.

 Developers: Gaël Lane Lépine, Samuel Béland, Olivier Bélanger, Nicolas Masson

 SpatGRIS is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 SpatGRIS is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with SpatGRIS.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "Data/sg_constants.hpp"

#include <JuceHeader.h>

#if JUCE_LINUX || JUCE_BSD
    #include <cstdlib>
#endif

namespace gris
{
/* Runtime control of the synthetic JACK device added by
   patches/0002-juce-jack-virtual-ports.patch.

   That patch makes juce_JackAudio.cpp advertise a device named "Virtual ports
   (patch manually)" whose port count comes from JUCE_JACK_VIRTUAL_INPUTS and
   JUCE_JACK_VIRTUAL_OUTPUTS. It reads them with getenv(), twice: in
   JackAudioIODeviceType::scanForDevices, to decide whether to list the device at
   all, and in JackAudioIODevice's constructor, which registers exactly one JACK
   port per name returned by get{Input,Output}ChannelNames(). Setting the
   variables from inside the process is therefore enough to change the count,
   provided the device object is afterwards destroyed and rebuilt rather than
   merely reopened -- see SettingsComponent::applyJackVirtualPortCounts().

   Without the patch these variables mean nothing to JUCE, so the controls do no
   harm on an unpatched build; the device simply never appears. Everything here
   is a no-op away from Linux and BSD, matching the patch's own guards.

   Header-only on purpose. A .cpp would have to be listed in SpatGRIS.jucer,
   which CI rewrites on a throwaway checkout to keep it byte-identical to
   upstream.
*/
namespace jackVirtualPorts
{
/* SpatGRIS cannot address more channels than this anyway, and it keeps a typo
   well below jack.max-client-ports (768 by default under pipewire-jack). */
constexpr int MAX_PORTS = 256;
static_assert(MAX_PORTS <= MAX_NUM_SOURCES && MAX_PORTS <= MAX_NUM_SPEAKERS,
              "a virtual port would be unreachable from SpatGRIS");

constexpr auto const * INPUTS_ENV_VAR = "JUCE_JACK_VIRTUAL_INPUTS";
constexpr auto const * OUTPUTS_ENV_VAR = "JUCE_JACK_VIRTUAL_OUTPUTS";

/* Must match JackVirtualDevice::deviceName in the patch. */
[[nodiscard]] inline juce::String deviceName()
{
    return "Virtual ports (patch manually)";
}

[[nodiscard]] inline constexpr bool isSupported()
{
#if JUCE_LINUX || JUCE_BSD
    return true;
#else
    return false;
#endif
}

//==============================================================================
[[nodiscard]] inline int readCount(char const * const varName)
{
    if (!isSupported()) {
        return 0;
    }

    auto const value{ juce::SystemStats::getEnvironmentVariable(varName, {}).trim() };
    if (value.isEmpty()) {
        return 0;
    }

    return juce::jlimit(0, MAX_PORTS, value.getIntValue());
}

[[nodiscard]] inline int getNumInputs()
{
    return readCount(INPUTS_ENV_VAR);
}

[[nodiscard]] inline int getNumOutputs()
{
    return readCount(OUTPUTS_ENV_VAR);
}

//==============================================================================
inline void setEnvCount([[maybe_unused]] char const * const varName, [[maybe_unused]] int const count)
{
#if JUCE_LINUX || JUCE_BSD
    if (count > 0) {
        auto const value{ juce::String{ count } };
        ::setenv(varName, value.toRawUTF8(), 1);
    } else {
        // Unset rather than "0": the patch treats absent and non-positive alike,
        // but an unset variable also restores plain upstream behaviour for
        // anything else that reads it.
        ::unsetenv(varName);
    }
#endif
}

//==============================================================================
/* Stored apart from the main settings file for two reasons: Configuration::save
   clears every key before writing its single blob, and the main file's name
   embeds the version, so the counts would silently reset on a version bump. */
[[nodiscard]] inline juce::PropertiesFile::Options storageOptions()
{
    juce::PropertiesFile::Options options{};
    options.applicationName = "SpatGRIS-jack-virtual-ports";
    options.commonToAllUsers = false;
    options.filenameSuffix = "xml";
    options.folderName = "GRIS";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    options.ignoreCaseOfKeyNames = true;
    options.osxLibrarySubFolder = "Application Support";
    return options;
}

constexpr auto const * INPUTS_KEY = "jackVirtualInputs";
constexpr auto const * OUTPUTS_KEY = "jackVirtualOutputs";

//==============================================================================
/* Set the counts for the rest of this run and remember them for the next one. */
inline void setCounts(int const numInputs, int const numOutputs)
{
    auto const inputs{ juce::jlimit(0, MAX_PORTS, numInputs) };
    auto const outputs{ juce::jlimit(0, MAX_PORTS, numOutputs) };

    setEnvCount(INPUTS_ENV_VAR, inputs);
    setEnvCount(OUTPUTS_ENV_VAR, outputs);

    juce::PropertiesFile storage{ storageOptions() };
    storage.setValue(INPUTS_KEY, inputs);
    storage.setValue(OUTPUTS_KEY, outputs);
    storage.saveIfNeeded();
}

//==============================================================================
/* Call once before the audio device is first scanned. Does nothing until the
   settings have been used at least once, so an untouched install still takes its
   counts from /etc/default/spatgris or ~/.config/spatgris.conf, as before. */
inline void applyStoredCounts()
{
    if (!isSupported()) {
        return;
    }

    juce::PropertiesFile storage{ storageOptions() };
    if (!storage.containsKey(INPUTS_KEY) && !storage.containsKey(OUTPUTS_KEY)) {
        return;
    }

    setEnvCount(INPUTS_ENV_VAR, juce::jlimit(0, MAX_PORTS, storage.getIntValue(INPUTS_KEY)));
    setEnvCount(OUTPUTS_ENV_VAR, juce::jlimit(0, MAX_PORTS, storage.getIntValue(OUTPUTS_KEY)));
}

} // namespace jackVirtualPorts

} // namespace gris

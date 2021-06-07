/*
 This file is part of SpatGRIS.

 Developers: Samuel Béland, Olivier Bélanger, Nicolas Masson

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

#include "AbstractSpatAlgorithm.hpp"
#include "lbap.hpp"

struct LbapSpatData {
    SpeakersSpatGains gains{};
    float lbapSourceDistance{};
};

using LbapSpatDataQueue = AtomicExchanger<LbapSpatData>;

struct LbapSourceData {
    LbapSpatDataQueue dataQueue{};
    LbapSpatDataQueue::Ticket * currentData{};
    LbapSourceAttenuationState attenuationState{};
    SpeakersSpatGains lastGains{};
};

//==============================================================================
class LbapSpatAlgorithm final : public AbstractSpatAlgorithm
{
    mField mField{};
    StrongArray<source_index_t, LbapSourceData, MAX_NUM_SOURCES> mData{};

public:
    //==============================================================================
    explicit LbapSpatAlgorithm(SpeakersData const & speakers);
    //==============================================================================
    void updateSpatData(source_index_t sourceIndex, SourceData const & sourceData) noexcept override;
    void process(AudioConfig const & config,
                 SourceAudioBuffer & sourceBuffer,
                 SpeakerAudioBuffer & speakersBuffer,
                 SourcePeaks const & sourcesPeaks,
                 SpeakersAudioConfig const * altSpeakerConfig) override;
    [[nodiscard]] juce::Array<Triplet> getTriplets() const noexcept override;
    [[nodiscard]] bool hasTriplets() const noexcept override { return false; }

private:
    JUCE_LEAK_DETECTOR(LbapSpatAlgorithm)
};

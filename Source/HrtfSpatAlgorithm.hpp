/*
 This file is part of SpatGRIS.

 Developers: Samuel B�land, Olivier B�langer, Nicolas Masson

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
#include "SpatMode.hpp"

//==============================================================================
class HrtfSpatAlgorithm final : public AbstractSpatAlgorithm
{
    std::unique_ptr<AbstractSpatAlgorithm> mInnerAlgorithm{};

public:
    //==============================================================================
    HrtfSpatAlgorithm(SpeakerSetup const & speakerSetup, SourcesData const & sources, SpatData & spatData);
    //==============================================================================
    HrtfSpatAlgorithm() = delete;
    ~HrtfSpatAlgorithm() override = default;
    //==============================================================================
    HrtfSpatAlgorithm(HrtfSpatAlgorithm const &) = delete;
    HrtfSpatAlgorithm(HrtfSpatAlgorithm &&) = delete;
    HrtfSpatAlgorithm & operator=(HrtfSpatAlgorithm const &) = delete;
    HrtfSpatAlgorithm & operator=(HrtfSpatAlgorithm &&) = delete;
    //==============================================================================
    void updateSpatData(SourceData const & sourceData, SourceSpatData & spatData) const noexcept override;
    [[nodiscard]] juce::Array<Triplet> getTriplets() const noexcept override;
    [[nodiscard]] bool hasTriplets() const noexcept override;

private:
    //==============================================================================
    JUCE_LEAK_DETECTOR(HrtfSpatAlgorithm)
};
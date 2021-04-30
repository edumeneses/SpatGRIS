#pragma once

#include "AbstractSpatAlgorithm.hpp"

#include "lbap.hpp"

//==============================================================================
class LbapSpatAlgorithm final : public AbstractSpatAlgorithm
{
    lbap_field mData{};

public:
    //==============================================================================
    explicit LbapSpatAlgorithm(SpeakersData const & speakers);
    //==============================================================================
    [[nodiscard]] void computeSpeakerGains(SourceData const & source,
                                           SpeakersSpatGains & gains) const noexcept override;
    [[nodiscard]] juce::Array<Triplet> getTriplets() const noexcept override;
    [[nodiscard]] bool hasTriplets() const noexcept override { return false; }
};
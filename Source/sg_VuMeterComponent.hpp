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

#include "sg_GrisLookAndFeel.hpp"
#include "sg_LogicStrucs.hpp"
#include "sg_MinSizedComponent.hpp"

#include <JuceHeader.h>

static dbfs_t constexpr MIN_LEVEL_COMP{ -60.0f };
static dbfs_t constexpr MAX_LEVEL_COMP{ 0.0f };

class GrisLookAndFeel;

//============================ VuMeterComponent ================================
class VuMeterComponent final : public juce::Component
{
    //==============================================================================
    SmallGrisLookAndFeel & mLookAndFeel;

    juce::ColourGradient mColorGrad;
    juce::Image mVuMeterBit;
    juce::Image mVuMeterBackBit;
    juce::Image mVuMeterMutedBit;
    bool mIsClipping{};
    bool mIsMuted{};
    dbfs_t mLevel{ MIN_LEVEL_COMP };

public:
    static constexpr auto MIN_HEIGHT = 140;
    //==============================================================================
    explicit VuMeterComponent(SmallGrisLookAndFeel & lookAndFeel) : mLookAndFeel(lookAndFeel) {}
    ~VuMeterComponent() override = default;
    //==============================================================================
    VuMeterComponent(VuMeterComponent const &) = delete;
    VuMeterComponent(VuMeterComponent &&) = delete;
    VuMeterComponent & operator=(VuMeterComponent const &) = delete;
    VuMeterComponent & operator=(VuMeterComponent &&) = delete;
    //==============================================================================
    void resized() override;
    void resetClipping();
    void setLevel(dbfs_t level);
    void setMuted(bool muted);
    //==============================================================================
    void paint(juce::Graphics & g) override;
    void mouseDown(const juce::MouseEvent & e) override;

private:
    //==============================================================================
    JUCE_LEAK_DETECTOR(VuMeterComponent)
};

//==============================================================================
class AbstractSliceComponent
    : public MinSizedComponent
    , public juce::ToggleButton::Listener
{
protected:
    static constexpr auto ID_BUTTON_HEIGHT = 17;
    static constexpr auto MUTE_AND_SOLO_BUTTONS_HEIGHT = 15;
    static constexpr auto INNER_ELEMENTS_PADDING = 1;

    SmallGrisLookAndFeel & mLookAndFeel;

    VuMeterComponent mLevelBox;
    juce::Label mIdLabel;
    juce::TextButton mIdButton;
    juce::Label mMuteLabel;
    juce::TextButton mMuteButton;
    juce::Label mSoloLabel;
    juce::TextButton mSoloButton;

public:
    //==============================================================================
    explicit AbstractSliceComponent(juce::String const & id, SmallGrisLookAndFeel & lookAndFeel);
    //==============================================================================
    AbstractSliceComponent() = delete;
    ~AbstractSliceComponent() override = default;
    //==============================================================================
    AbstractSliceComponent(AbstractSliceComponent const &) = delete;
    AbstractSliceComponent(AbstractSliceComponent &&) = delete;
    AbstractSliceComponent & operator=(AbstractSliceComponent const &) = delete;
    AbstractSliceComponent & operator=(AbstractSliceComponent &&) = delete;
    //==============================================================================
    void setLevel(dbfs_t const level) { mLevelBox.setLevel(level); }
    void resetClipping() { mLevelBox.resetClipping(); }
    void setState(PortState state, bool soloMode);
    //==============================================================================
    void resized() override;
    [[nodiscard]] int getMinWidth() const noexcept final;

private:
    //==============================================================================
    JUCE_LEAK_DETECTOR(AbstractSliceComponent)
}; // class LevelComponent

//==============================================================================
class SourceSliceComponent final
    : public AbstractSliceComponent
    , public juce::ChangeListener
{
public:
    static juce::String const NO_DIRECT_OUT_TEXT;
    //==============================================================================
    class Owner
    {
    public:
        virtual ~Owner() = default;
        virtual void setSourceDirectOut(source_index_t sourceIndex, tl::optional<output_patch_t> outputPatch) = 0;
        virtual void setSourceColor(source_index_t sourceIndex, juce::Colour colour) = 0;
        virtual void setSourceState(source_index_t sourceIndex, PortState state) = 0;
        virtual void setSourceHybridSpatMode(source_index_t sourceIndex, SpatMode spatMode) = 0;
        [[nodiscard]] virtual SpeakersData const & getSpeakersData() const = 0;
    };

private:
    static constexpr auto DIRECT_OUT_BUTTON_HEIGHT = 17;
    //==============================================================================
    source_index_t mSourceIndex{};
    Owner & mOwner;

    juce::Label mDomeLabel;
    juce::TextButton mDomeButton;
    juce::Label mCubeLabel;
    juce::TextButton mCubeButton;
    juce::TextButton mDirectOutButton;

public:
    //==============================================================================
    SourceSliceComponent(source_index_t sourceIndex,
                         tl::optional<output_patch_t> directOut,
                         SpatMode projectSpatMode,
                         SpatMode hybridSpatMode,
                         juce::Colour colour,
                         Owner & owner,
                         SmallGrisLookAndFeel & lookAndFeel);
    ~SourceSliceComponent() override = default;
    //==============================================================================
    SourceSliceComponent(SourceSliceComponent const &) = delete;
    SourceSliceComponent(SourceSliceComponent &&) = delete;
    SourceSliceComponent & operator=(SourceSliceComponent const &) = delete;
    SourceSliceComponent & operator=(SourceSliceComponent &&) = delete;
    //==============================================================================
    void setDirectOut(tl::optional<output_patch_t> outputPatch);
    void setSourceColour(juce::Colour colour);
    void setProjectSpatMode(SpatMode spatMode);
    void setHybridSpatMode(SpatMode spatMode);
    //==============================================================================
    // overrides
    void buttonClicked(juce::Button * button) override;
    void resized() override;
    void changeListenerCallback(juce::ChangeBroadcaster * source) override;
    [[nodiscard]] int getMinHeight() const noexcept override;
    void mouseUp(const juce::MouseEvent & event) override;

private:
    //==============================================================================
    void muteButtonClicked() const;
    void soloButtonClicked() const;
    void colorSelectorLeftButtonClicked();
    void colorSelectorRightButtonClicked() const;
    void directOutButtonClicked() const;
    void domeButtonClicked() const;
    void cubeButtonClicked() const;
    [[nodiscard]] juce::Colour getSourceColor() const;
    //==============================================================================
    JUCE_LEAK_DETECTOR(AbstractSliceComponent)
}; // class LevelComponent

//==============================================================================
class SpeakerSliceComponent final : public AbstractSliceComponent
{
public:
    //==============================================================================
    class Owner
    {
    public:
        virtual ~Owner() = default;
        virtual void setSelectedSpeakers(juce::Array<output_patch_t> selection) = 0;
        virtual void setSpeakerState(output_patch_t outputPatch, PortState state) = 0;
    };

private:
    //==============================================================================
    output_patch_t mOutputPatch{};
    Owner & mOwner;

public:
    //==============================================================================
    SpeakerSliceComponent(output_patch_t outputPatch, Owner & owner, SmallGrisLookAndFeel & lookAndFeel);
    ~SpeakerSliceComponent() override = default;
    //==============================================================================
    SpeakerSliceComponent(SpeakerSliceComponent const &) = delete;
    SpeakerSliceComponent(SpeakerSliceComponent &&) = delete;
    SpeakerSliceComponent & operator=(SpeakerSliceComponent const &) = delete;
    SpeakerSliceComponent & operator=(SpeakerSliceComponent &&) = delete;
    //==============================================================================
    void setSelected(bool value);
    //==============================================================================
    void buttonClicked(juce::Button * button) override;
    [[nodiscard]] int getMinHeight() const noexcept override;

private:
    //==============================================================================
    JUCE_LEAK_DETECTOR(SpeakerSliceComponent)
}; // class LevelComponent

//==============================================================================
class StereoSliceComponent final : public AbstractSliceComponent
{
public:
    StereoSliceComponent(juce::String const & id, SmallGrisLookAndFeel & lookAndFeel);
    ~StereoSliceComponent() override = default;

    StereoSliceComponent(StereoSliceComponent const &) = delete;
    StereoSliceComponent(StereoSliceComponent &&) = delete;
    StereoSliceComponent & operator=(StereoSliceComponent const &) = delete;
    StereoSliceComponent & operator=(StereoSliceComponent &&) = delete;

    [[nodiscard]] int getMinHeight() const noexcept override;
    void buttonClicked(juce::Button *) override {}
};
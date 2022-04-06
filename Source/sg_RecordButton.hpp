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

#include "sg_MinSizedComponent.hpp"

namespace gris
{
class GrisLookAndFeel;

//==============================================================================
class RecordButton final
    : public MinSizedComponent
    , juce::Timer
{
public:
    enum class State { ready, recording };
    //==============================================================================
    class Listener
    {
    public:
        Listener() = default;
        virtual ~Listener() = default;
        SG_DEFAULT_COPY_AND_MOVE(Listener)
        //==============================================================================
        virtual void recordButtonPressed() = 0;

    private:
        //==============================================================================
        JUCE_LEAK_DETECTOR(Listener)
    };

private:
    //==============================================================================
    Listener & mListener;
    State mState{ State::ready };
    bool mBlinkState{};

    juce::Rectangle<int> mActiveBounds{};
    juce::Label mRecordedTime{};
    juce::int64 mTimeRecordingStarted{};

public:
    //==============================================================================
    explicit RecordButton(Listener & listener, GrisLookAndFeel & lookAndFeel);
    ~RecordButton() override = default;
    SG_DELETE_COPY_AND_MOVE(RecordButton)
    //==============================================================================
    void setState(State state);
    //==============================================================================
    void paint(juce::Graphics & g) override;
    void resized() override;
    void mouseUp(juce::MouseEvent const & event) override;
    void mouseMove(const juce::MouseEvent & event) override;
    void mouseExit(const juce::MouseEvent & event) override;
    [[nodiscard]] int getMinWidth() const noexcept override;
    [[nodiscard]] int getMinHeight() const noexcept override;

private:
    //==============================================================================
    void updateRecordedTime();
    //==============================================================================
    void timerCallback() override;
    //==============================================================================
    JUCE_LEAK_DETECTOR(RecordButton)
};

} // namespace gris
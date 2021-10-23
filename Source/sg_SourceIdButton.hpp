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

#include "sg_SmallToggleButton.hpp"
#include "sg_SourceIndex.hpp"

class SmallGrisLookAndFeel;

//==============================================================================
class SourceIdButton final
    : public MinSizedComponent
    , private SmallToggleButton::Listener
    , private juce::ChangeListener
{
public:
    //==============================================================================
    class Listener
    {
    public:
        Listener() = default;
        virtual ~Listener() = default;
        //==============================================================================
        Listener(Listener const &) = delete;
        Listener(Listener &&) = delete;
        Listener & operator=(Listener const &) = delete;
        Listener & operator=(Listener &&) = delete;
        //==============================================================================
        virtual void sourceIdButtonColorChanged(SourceIdButton * button, juce::Colour color) = 0;
        virtual void sourceIdButtonCopyColorToNextSource(SourceIdButton * button, juce::Colour color) = 0;
    };

private:
    //==============================================================================
    Listener & mListener;
    SmallGrisLookAndFeel & mLookAndFeel;
    SmallToggleButton mButton;

public:
    //==============================================================================
    SourceIdButton(source_index_t sourceIndex,
                   juce::Colour color,
                   Listener & listener,
                   SmallGrisLookAndFeel & lookAndFeel);
    ~SourceIdButton() override = default;
    //==============================================================================
    SourceIdButton(SourceIdButton const &) = delete;
    SourceIdButton(SourceIdButton &&) = delete;
    SourceIdButton & operator=(SourceIdButton const &) = delete;
    SourceIdButton & operator=(SourceIdButton &&) = delete;
    //==============================================================================
    void setColor(juce::Colour const & color);
    //==============================================================================
    [[nodiscard]] int getMinWidth() const noexcept override { return 0; }
    [[nodiscard]] int getMinHeight() const noexcept override { return 0; }

private:
    //==============================================================================
    void changeListenerCallback(juce::ChangeBroadcaster * source) override;
    void smallButtonClicked(SmallToggleButton * button, bool state, bool isLeftMouseButton) override;
    //==============================================================================
    JUCE_LEAK_DETECTOR(SourceIdButton)
};
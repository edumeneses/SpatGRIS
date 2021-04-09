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

#include "LogicStrucs.hpp"
#include "constants.hpp"
#include "lib/tl/optional.hpp"

#include "macros.h"
DISABLE_WARNINGS
#include <JuceHeader.h>
ENABLE_WARNINGS

#include "AboutWindow.h"
#include "AbstractSpatAlgorithm.hpp"
#include "AudioProcessor.h"
#include "Box.h"
#include "Configuration.h"
#include "EditSpeakersWindow.h"
#include "FlatViewWindow.h"
#include "InputModel.h"
#include "OscInput.h"
#include "OscLogWindow.h"
#include "OwnedMap.hpp"
#include "SettingsWindow.h"
#include "SpeakerModel.h"
#include "SpeakerViewComponent.h"
#include "StrongTypes.hpp"

class MainWindow;

class AudioDeviceManagerListener : public juce::ChangeListener
{
protected:
    virtual void audioParametersChanged() = 0;

private:
    void changeListenerCallback([[maybe_unused]] juce::ChangeBroadcaster * source) override
    {
        jassert(dynamic_cast<juce::AudioDeviceManager *>(source));
        audioParametersChanged();
    }
};

//==============================================================================
// This component lives inside our window, and this is where you should put all your controls and content.
class MainContentComponent final
    : public juce::Component
    , public juce::MenuBarModel
    , public juce::ApplicationCommandTarget
    , public juce::Button::Listener
    , public juce::TextEditor::Listener
    , public juce::Slider::Listener
    , public juce::ComboBox::Listener
    , private AudioDeviceManagerListener
    , private juce::Timer
{
    std::unique_ptr<AudioProcessor> mAudioProcessor{};

    // Speakers.
    juce::Array<Triplet> mTriplets{};
    OwnedMap<output_patch_t, SpeakerModel> mSpeakers{};
    juce::Array<output_patch_t> mSpeakersDisplayOrder{};

    // Sources.
    juce::OwnedArray<InputModel> mInputModels{};
    juce::CriticalSection mInputsLock{};

    // Open Sound Control.
    std::unique_ptr<OscInput> mOscReceiver{};

    juce::String mConfigurationName{};

    juce::File mCurrentSpeakerSetup{};

    // Windows.
    std::unique_ptr<EditSpeakersWindow> mEditSpeakersWindow{};
    std::unique_ptr<SettingsWindow> mPropertiesWindow{};
    std::unique_ptr<FlatViewWindow> mFlatViewWindow{};
    std::unique_ptr<AboutWindow> mAboutWindow{};
    std::unique_ptr<OscLogWindow> mOscLogWindow{};

    // 3 Main Boxes.
    std::unique_ptr<Box> mMainUiBox{};
    std::unique_ptr<Box> mInputsUiBox{};
    std::unique_ptr<Box> mOutputsUiBox{};
    std::unique_ptr<Box> mControlUiBox{};

    // Component in Box 3.
    std::unique_ptr<juce::Label> mCpuUsageLabel{};
    std::unique_ptr<juce::Label> mCpuUsageValue{};
    std::unique_ptr<juce::Label> mSampleRateLabel{};
    std::unique_ptr<juce::Label> mBufferSizeLabel{};
    std::unique_ptr<juce::Label> mChannelCountLabel{};

    std::unique_ptr<juce::ComboBox> mSpatModeCombo{};

    std::unique_ptr<juce::Slider> mMasterGainOutSlider{};
    std::unique_ptr<juce::Slider> mInterpolationSlider{};

    std::unique_ptr<juce::TextEditor> mAddInputsTextEditor{};

    std::unique_ptr<juce::TextButton> mStartRecordButton{};
    std::unique_ptr<juce::TextEditor> mMinRecordTextEditor{};
    std::unique_ptr<juce::Label> mTimeRecordedLabel{};
    std::unique_ptr<juce::TextButton> mInitRecordButton{};

    // UI Components.
    std::unique_ptr<SpeakerViewComponent> mSpeakerViewComponent{};
    juce::StretchableLayoutManager mVerticalLayout{};
    std::unique_ptr<juce::StretchableLayoutResizerBar> mVerticalDividerBar{};

    // App splash screen.
    std::unique_ptr<juce::SplashScreen> mSplashScreen{};

    // Flags.
    bool mIsProcessForeground{ true };
    //==============================================================================
    // Look-and-feel.
    GrisLookAndFeel & mLookAndFeel;
    SmallGrisLookAndFeel & mSmallLookAndFeel;

    MainWindow & mMainWindow;

    std::unique_ptr<juce::MenuBarComponent> mMenuBar{};
    //==============================================================================
    // App user settings.

    Configuration mConfiguration;
    juce::Rectangle<int> mFlatViewWindowRect{};

    // App states.
    bool mNeedToSavePreset{ false };
    bool mNeedToSaveSpeakerSetup{ false };

    SpatGrisData mData;
    std::unique_ptr<AbstractSpatAlgorithm> mSpatAlgorithm{};

public:
    //==============================================================================
    MainContentComponent(MainWindow & mainWindow,
                         GrisLookAndFeel & grisLookAndFeel,
                         SmallGrisLookAndFeel & smallGrisLookAndFeel);
    //==============================================================================
    MainContentComponent() = delete;
    ~MainContentComponent() override;

    MainContentComponent(MainContentComponent const &) = delete;
    MainContentComponent(MainContentComponent &&) = delete;

    MainContentComponent & operator=(MainContentComponent const &) = delete;
    MainContentComponent & operator=(MainContentComponent &&) = delete;
    //==============================================================================
    // Exit application.
    [[nodiscard]] bool exitApp() const;

    void setShowTriplets(bool state);

    // other
    [[nodiscard]] bool isTripletsShown() const { return mData.projectData.viewSettings.showSpeakerTriplets; }
    [[nodiscard]] bool needToSaveSpeakerSetup() const { return mNeedToSaveSpeakerSetup; }
    [[nodiscard]] bool isSpanShown() const { return true; } // TODO
    [[nodiscard]] bool isSourceLevelShown() const { return mData.projectData.viewSettings.showSourceActivity; }
    [[nodiscard]] bool isSpeakerLevelShown() const { return mData.projectData.viewSettings.showSpeakerLevels; }

    void setNeedToSaveSpeakerSetup(bool const state) { mNeedToSaveSpeakerSetup = state; }

    void setNeedToComputeVbap(bool const state) { jassertfalse; }

    void setNumInputs(int numInputs, bool const updateTextInput);

    // Speakers.
    [[nodiscard]] auto & getSpeakers() { return mSpeakers; }
    [[nodiscard]] auto const & getSpeakers() const { return mSpeakers; }
    [[nodiscard]] auto const & getSpeakersDisplayOrder() const { return mSpeakersDisplayOrder; }

    [[nodiscard]] SpeakerModel * getSpeakerFromOutputPatch(output_patch_t out);
    [[nodiscard]] SpeakerModel const * getSpeakerFromOutputPatch(output_patch_t out) const;

    SpeakerModel & addSpeaker();
    void insertSpeaker(int position);
    void removeSpeaker(output_patch_t outputPatch);
    void setSourceDirectOut(source_index_t const, output_patch_t) const;
    void reorderSpeakers(juce::Array<output_patch_t> newOrder);

    [[nodiscard]] dbfs_t getSourcePeak(source_index_t sourceIndex) const;
    [[nodiscard]] float getSourceAlpha(source_index_t sourceIndex) const;
    [[nodiscard]] dbfs_t getSpeakerPeak(output_patch_t outputPatch) const;
    [[nodiscard]] float getSpeakerAlpha(output_patch_t outputPatch) const;

    // Sources.
    [[nodiscard]] juce::OwnedArray<InputModel> & getSourceInputs() { return mInputModels; }
    [[nodiscard]] juce::OwnedArray<InputModel> const & getSourceInputs() const { return mInputModels; }

    [[nodiscard]] juce::CriticalSection const & getInputsLock() const { return mInputsLock; }

    [[nodiscard]] bool isRadiusNormalized() const;

    [[nodiscard]] AudioProcessor & getAudioProcessor() { return *mAudioProcessor; }
    [[nodiscard]] AudioProcessor const & getAudioProcessor() const { return *mAudioProcessor; }

    // VBAP triplets.
    [[nodiscard]] juce::Array<Triplet> & getTriplets() { return mTriplets; }
    [[nodiscard]] juce::Array<Triplet> const & getTriplets() const { return mTriplets; }

    // Speaker selections.
    void selectSpeaker(tl::optional<output_patch_t> outputPatch);
    void selectTripletSpeaker(output_patch_t outputPatch);

    // Mute - solo.
    void setSourceState(source_index_t sourceIndex, PortState state);
    void setSpeakerState(output_patch_t outputPatch, PortState state);

    // Input - output amplitude levels.
    [[nodiscard]] float getPeak(output_patch_t outputPatch) const;
    [[nodiscard]] float getPeak(source_index_t sourceIndex) const;
    [[nodiscard]] float getAlpha(output_patch_t outputPatch) const;
    [[nodiscard]] float getAlpha(source_index_t sourceIndex) const;

    // Called when the speaker setup has changed.
    bool refreshSpeakers();

    // Open - save.
    void reloadXmlFileSpeaker();
    void saveProperties(SpatGrisAppData const & appData);

    // Screen refresh timer.
    void handleTimer(bool state);
    void handleSaveAsSpeakerSetup();

    // Close windows other than the main one.
    void closeSpeakersConfigurationWindow();
    void closePropertiesWindow() { mPropertiesWindow.reset(); }
    void closeFlatViewWindow() { mFlatViewWindow.reset(); }
    void closeAboutWindow() { mAboutWindow.reset(); }
    void closeOscLogWindow() { mOscLogWindow.reset(); }

    [[nodiscard]] auto const & getConfiguration() const { return mConfiguration; }
    [[nodiscard]] SpatMode getModeSelected() const;
    void setOscLogging(const juce::OSCMessage & message) const;
    void updateSourceData(int sourceDataIndex, InputModel & input) const;
    //==============================================================================
    void timerCallback() override;
    void paint(juce::Graphics & g) override;
    void resized() override;
    void buttonClicked(juce::Button * button) override;
    void sliderValueChanged(juce::Slider * slider) override;
    void textEditorFocusLost(juce::TextEditor & textEditor) override;
    void textEditorReturnKeyPressed(juce::TextEditor & textEditor) override;
    void comboBoxChanged(juce::ComboBox * comboBoxThatHasChanged) override;
    void menuItemSelected(int menuItemId, int /*topLevelMenuIndex*/) override;
    [[nodiscard]] juce::StringArray getMenuBarNames() override;
    [[nodiscard]] juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String & /*menuName*/) override;

private:
    //==============================================================================
    // Widget creation helpers.
    juce::TextEditor * addTextEditor(juce::String const & s,
                                     juce::String const & emptyS,
                                     juce::String const & tooltip,
                                     int x,
                                     int y,
                                     int w,
                                     int h,
                                     juce::Component * into,
                                     int wLab = 80);
    juce::Label * addLabel(const juce::String & s,
                           const juce::String & tooltip,
                           int x,
                           int y,
                           int w,
                           int h,
                           Component * into) const;
    juce::TextButton *
        addButton(const juce::String & s, const juce::String & tooltip, int x, int y, int w, int h, Component * into);
    juce::ToggleButton * addToggleButton(const juce::String & s,
                                         const juce::String & tooltip,
                                         int x,
                                         int y,
                                         int w,
                                         int h,
                                         Component * into,
                                         bool toggle = false);
    juce::Slider *
        addSlider(const juce::String & s, const juce::String & tooltip, int x, int y, int w, int h, Component * into);
    juce::ComboBox *
        addComboBox(const juce::String & s, const juce::String & tooltip, int x, int y, int w, int h, Component * into);
    //==============================================================================
    // MenuBar handlers.
    void handleNew();
    void handleOpenProject();
    void handleSaveProject() const;
    void handleSaveAsProject() const;
    void handleOpenSpeakerSetup();
    void handleShowSpeakerEditWindow();
    void handleShowPreferences();
    void handleShowAbout();
    void handleShow2DView();
    void handleShowNumbers();
    void handleShowSpeakers();
    void handleShowTriplets();
    void handleShowSourceLevel();
    void handleShowSpeakerLevel();
    void handleShowSphere();
    void handleResetInputPositions();
    void handleResetMeterClipping();
    void handleShowOscLogView();
    void handleInputColours();

    void setShowNumbers(bool state);
    void setShowSpeakers(bool state);
    void setTripletsFromVbap();

    [[nodiscard]] output_patch_t getMaxSpeakerOutputPatch() const;
    [[nodiscard]] bool tripletExists(Triplet const & tri, int & pos) const;
    [[nodiscard]] bool validateShowTriplets() const;
    [[nodiscard]] bool isProjectModified() const;
    //==============================================================================
    // Open - save.
    void openXmlFileSpeaker(juce::File const & file, tl::optional<SpatMode> forceSpatMode = tl::nullopt);
    void openProject(juce::File const & file);
    void getProjectData(juce::XmlElement * xml) const;
    void saveProject(juce::String const & path) const;
    void saveSpeakerSetup(juce::String const & path);
    void setCurrentSpeakerSetup(juce::File const & file);
    void setTitle() const;

    [[nodiscard]] bool initRecording() const;
    //==============================================================================
    // OVERRIDES
    void audioParametersChanged() override;

    void getAllCommands(juce::Array<juce::CommandID> & commands) override;
    void getCommandInfo(juce::CommandID commandId, juce::ApplicationCommandInfo & result) override;
    [[nodiscard]] juce::ApplicationCommandTarget * getNextCommandTarget() override;
    [[nodiscard]] bool perform(juce::ApplicationCommandTarget::InvocationInfo const & info) override;
    //==============================================================================
    static void handleOpenManual();
    //==============================================================================
    JUCE_LEAK_DETECTOR(MainContentComponent)
};

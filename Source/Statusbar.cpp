/*
 // Copyright (c) 2021-2022 Timothy Schoen.
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Utility/Config.h"
#include "Utility/Fonts.h"

#include "Statusbar.h"
#include "LookAndFeel.h"

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Canvas.h"
#include "Connection.h"

#include "Dialogs/OverlayDisplaySettings.h"
#include "Dialogs/SnapSettings.h"
#include "Dialogs/AlignmentTools.h"

#include "Utility/ArrowPopupMenu.h"

class OversampleSelector : public TextButton {

    class OversampleSettingsPopup : public Component {
    public:
        std::function<void(int)> onChange = [](int) {};
        std::function<void()> onClose = []() {};

        OversampleSettingsPopup(int currentSelection)
        {
            title.setText("Oversampling factor", dontSendNotification);
            title.setFont(Fonts::getBoldFont().withHeight(14.0f));
            title.setJustificationType(Justification::centred);
            addAndMakeVisible(title);

            one.setConnectedEdges(ConnectedOnRight);
            two.setConnectedEdges(ConnectedOnLeft | ConnectedOnRight);
            four.setConnectedEdges(ConnectedOnLeft | ConnectedOnRight);
            eight.setConnectedEdges(ConnectedOnLeft);

            auto buttons = Array<TextButton*> { &one, &two, &four, &eight };

            int i = 0;
            for (auto* button : buttons) {
                button->setRadioGroupId(hash("oversampling_selector"));
                button->setClickingTogglesState(true);
                button->onClick = [this, i]() {
                    onChange(i);
                };

                button->setColour(TextButton::textColourOffId, findColour(PlugDataColour::popupMenuTextColourId));
                button->setColour(TextButton::textColourOnId, findColour(PlugDataColour::popupMenuActiveTextColourId));
                button->setColour(TextButton::buttonColourId, findColour(PlugDataColour::popupMenuBackgroundColourId));
                button->setColour(TextButton::buttonOnColourId, findColour(PlugDataColour::popupMenuActiveBackgroundColourId));

                addAndMakeVisible(button);
                i++;
            }

            buttons[currentSelection]->setToggleState(true, dontSendNotification);

            setSize(180, 50);
        }

        ~OversampleSettingsPopup()
        {
            onClose();
        }

    private:
        void resized() override
        {
            auto b = getLocalBounds().reduced(4, 4);
            auto titleBounds = b.removeFromTop(22);

            title.setBounds(titleBounds.translated(0, -2));

            auto buttonWidth = b.getWidth() / 4;

            one.setBounds(b.removeFromLeft(buttonWidth));
            two.setBounds(b.removeFromLeft(buttonWidth).expanded(1, 0));
            four.setBounds(b.removeFromLeft(buttonWidth).expanded(1, 0));
            eight.setBounds(b.removeFromLeft(buttonWidth).expanded(1, 0));
        }

        Label title;
        TextButton one = TextButton("1x");
        TextButton two = TextButton("2x");
        TextButton four = TextButton("4x");
        TextButton eight = TextButton("8x");
    };

public:
    OversampleSelector(PluginProcessor* pd)
    {
        onClick = [this, pd]() {
            auto selection = log2(getButtonText().upToLastOccurrenceOf("x", false, false).getIntValue());
            auto* editor = dynamic_cast<PluginEditor*>(pd->getActiveEditor());
            auto oversampleSettings = std::make_unique<OversampleSettingsPopup>(selection);
            auto bounds = editor->getLocalArea(this, getLocalBounds());

            oversampleSettings->onChange = [this, pd](int result) {
                setButtonText(String(1 << result) + "x");
                pd->setOversampling(result);
            };
            oversampleSettings->onClose = [this]() {
                repaint();
            };

            CallOutBox::launchAsynchronously(std::move(oversampleSettings), bounds, editor);
        };
    }

private:
    void paint(Graphics& g) override
    {
        auto buttonText = getButtonText();
        if (buttonText == "1x") {
            g.setColour(isMouseOverOrDragging() ? findColour(PlugDataColour::toolbarTextColourId).brighter(0.8f) : findColour(PlugDataColour::toolbarTextColourId));
        } else {
            g.setColour(isMouseOverOrDragging() ? findColour(PlugDataColour::toolbarActiveColourId).brighter(0.8f) : findColour(PlugDataColour::toolbarActiveColourId));
        }

        g.setFont(14.0f);
        g.drawText(buttonText, getLocalBounds(), Justification::centred);
    }
};

class VolumeSlider : public Slider {
public:
    VolumeSlider()
        : Slider(Slider::LinearHorizontal, Slider::NoTextBox)
    {
        setSliderSnapsToMousePosition(false);
    }

    void resized() override
    {
        setMouseDragSensitivity(getWidth() - (margin * 2));
    }

    void mouseMove(MouseEvent const& e) override
    {
        repaint();
        Slider::mouseMove(e);
    }

    void mouseUp(MouseEvent const& e) override
    {
        repaint();
        Slider::mouseUp(e);
    }

    void mouseDown(MouseEvent const& e) override
    {
        repaint();
        Slider::mouseDown(e);
    }

    void paint(Graphics& g) override
    {
        auto backgroundColour = findColour(PlugDataColour::levelMeterThumbColourId);

        auto value = getValue();
        auto thumbSize = getHeight() * 0.7f;
        auto position = Point<float>(margin + (value * (getWidth() - (margin * 2))), getHeight() * 0.5f);
        auto thumb = Rectangle<float>(thumbSize, thumbSize).withCentre(position);
        thumb = thumb.withSizeKeepingCentre(thumb.getWidth() - 12, thumb.getHeight());
        g.setColour(backgroundColour.darker(thumb.contains(getMouseXYRelative().toFloat()) ? 0.3f : 0.0f).withAlpha(0.8f));
        PlugDataLook::fillSmoothedRectangle(g, thumb, Corners::defaultCornerRadius * 0.5f);
    }

private:
    int margin = 18;
};

class LevelMeter : public Component
    , public StatusbarSource::Listener
    , public MultiTimer {
    float audioLevel[2] = { 0.0f, 0.0f };
    float peakLevel[2] = { 0.0f, 0.0f };

    int numChannels = 2;

    bool clipping[2] = { false, false };

    bool peakBarsFade[2] = { true, true };

    float fadeFactor = 0.98f;

    float lastPeak[2] = { 0.0f };
    float lastLevel[2] = { 0.0f };
    float repaintTheshold = 0.01f;

public:
    LevelMeter() = default;

    void audioLevelChanged(Array<float> peak) override
    {
        bool needsRepaint = false;
        for (int i = 0; i < 2; i++) {
            audioLevel[i] *= fadeFactor;
            if (peakBarsFade[i])
                peakLevel[i] *= fadeFactor;

            if (peak[i] > audioLevel[i]) {
                audioLevel[i] = peak[i];
                if (peak[i] >= 1.0f)
                    clipping[i] = true;
                else
                    clipping[i] = false;
            }
            if (peak[i] > peakLevel[i]) {
                peakLevel[i] = peak[i];
                peakBarsFade[i] = false;
                startTimer(i, 1700);
            }

            if (std::abs(peakLevel[i] - lastPeak[i]) > repaintTheshold
                || std::abs(audioLevel[i] - lastLevel[i]) > repaintTheshold
                || (peakLevel[i] == 0.0f && lastPeak[i] != 0.0f)
                || (audioLevel[i] == 0.0f && lastLevel[i] != 0.0f)) {
                lastPeak[i] = peakLevel[i];
                lastLevel[i] = audioLevel[i];

                needsRepaint = true;
            }
        }

        if (needsRepaint)
            repaint();
    }

    void timerCallback(int timerID) override
    {
        peakBarsFade[timerID] = true;
    }

    void paint(Graphics& g) override
    {
        auto height = getHeight() / 4.0f;
        auto barHeight = height * 0.6f;
        auto halfBarHeight = barHeight * 0.5f;
        auto width = getWidth() - 12.0f;
        auto x = 6.0f;

        auto outerBorderWidth = 2.0f;
        auto spacingFraction = 0.08f;
        auto doubleOuterBorderWidth = 2.0f * outerBorderWidth;
        auto bgHeight = getHeight() - doubleOuterBorderWidth;
        auto bgWidth = width - doubleOuterBorderWidth;
        auto meterWidth = width - bgHeight;
        auto barWidth = meterWidth - 2;
        auto leftOffset = x + (bgHeight * 0.5f);

        g.setColour(findColour(PlugDataColour::levelMeterBackgroundColourId));
        g.fillRoundedRectangle(x + outerBorderWidth + 4, outerBorderWidth, bgWidth - 8, bgHeight, Corners::defaultCornerRadius);

        for (int ch = 0; ch < numChannels; ch++) {
            auto barYPos = outerBorderWidth + ((ch + 1) * (bgHeight / 3.0f)) - halfBarHeight;
            auto barLength = jmin(audioLevel[ch] * barWidth, barWidth);
            auto peekPos = jmin(peakLevel[ch] * barWidth, barWidth);

            if (peekPos > 1) {
                g.setColour(clipping[ch] ? Colours::red : findColour(PlugDataColour::levelMeterActiveColourId));
                g.fillRect(leftOffset, barYPos, barLength, barHeight);
                g.fillRect(leftOffset + peekPos, barYPos, 1.0f, barHeight);
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};

class MidiBlinker : public Component
    , public StatusbarSource::Listener {

public:
    void paint(Graphics& g) override
    {
        Fonts::drawText(g, "MIDI", getLocalBounds().removeFromLeft(28).withTrimmedTop(1), findColour(ComboBox::textColourId), 11, Justification::centredRight);

        auto midiInRect = Rectangle<float>(38.0f, 9.5f, 15.0f, 3.0f);
        auto midiOutRect = Rectangle<float>(38.0f, 18.5f, 15.0f, 3.0f);

        g.setColour(blinkMidiIn ? findColour(PlugDataColour::levelMeterActiveColourId) : findColour(PlugDataColour::levelMeterBackgroundColourId));
        g.fillRoundedRectangle(midiInRect, 1.0f);

        g.setColour(blinkMidiOut ? findColour(PlugDataColour::levelMeterActiveColourId) : findColour(PlugDataColour::levelMeterBackgroundColourId));
        g.fillRoundedRectangle(midiOutRect, 1.0f);
    }

    void midiReceivedChanged(bool midiReceived) override
    {
        blinkMidiIn = midiReceived;
        repaint();
    };

    void midiSentChanged(bool midiSent) override
    {
        blinkMidiOut = midiSent;
        repaint();
    };

    bool blinkMidiIn = false;
    bool blinkMidiOut = false;
};

Statusbar::Statusbar(PluginProcessor* processor)
    : pd(processor)
{
    levelMeter = std::make_unique<LevelMeter>();
    midiBlinker = std::make_unique<MidiBlinker>();
    volumeSlider = std::make_unique<VolumeSlider>();
    oversampleSelector = std::make_unique<OversampleSelector>(processor);

    pd->statusbarSource->addListener(levelMeter.get());
    pd->statusbarSource->addListener(midiBlinker.get());
    pd->statusbarSource->addListener(this);

    setWantsKeyboardFocus(true);

    oversampleSelector->setTooltip("Set oversampling");
    oversampleSelector->getProperties().set("FontScale", 0.5f);
    oversampleSelector->setColour(ComboBox::outlineColourId, Colours::transparentBlack);

    oversampleSelector->setButtonText(String(1 << pd->oversampling) + "x");
    addAndMakeVisible(*oversampleSelector);

    powerButton.setButtonText(Icons::Power);
    protectButton.setButtonText(Icons::Protection);
    centreButton.setButtonText(Icons::Centre);
    fitAllButton.setButtonText(Icons::FitAll);

    powerButton.setTooltip("Enable/disable DSP");
    powerButton.setClickingTogglesState(true);
    powerButton.getProperties().set("Style", "SmallIcon");
    addAndMakeVisible(powerButton);

    powerButton.onClick = [this]() { powerButton.getToggleState() ? pd->startDSP() : pd->releaseDSP(); };

    powerButton.setToggleState(pd_getdspstate(), dontSendNotification);

    centreButton.setTooltip("Move view to origin");
    centreButton.getProperties().set("Style", "SmallIcon");
    centreButton.onClick = [this]() {
        auto* editor = dynamic_cast<PluginEditor*>(pd->getActiveEditor());
        if (auto* cnv = editor->getCurrentCanvas()) {
            cnv->jumpToOrigin();
        }
    };

    addAndMakeVisible(centreButton);

    fitAllButton.setTooltip("Zoom to fit all");
    fitAllButton.getProperties().set("Style", "SmallIcon");
    fitAllButton.onClick = [this]() {
        auto* editor = dynamic_cast<PluginEditor*>(pd->getActiveEditor());
        if (auto* cnv = editor->getCurrentCanvas()) {
            cnv->zoomToFitAll();
        }
    };

    addAndMakeVisible(fitAllButton);

    protectButton.setTooltip("Clip output signal and filter non-finite values");
    protectButton.getProperties().set("Style", "SmallIcon");
    protectButton.setClickingTogglesState(true);
    protectButton.setToggleState(SettingsFile::getInstance()->getProperty<int>("protected"), dontSendNotification);
    protectButton.onClick = [this]() {
        int state = protectButton.getToggleState();
        pd->setProtectedMode(state);
        SettingsFile::getInstance()->setProperty("protected", state);
    };
    addAndMakeVisible(protectButton);

    volumeSlider->setRange(0.0f, 1.0f);
    volumeSlider->setValue(0.8f);
    volumeSlider->setDoubleClickReturnValue(true, 0.8f);
    addAndMakeVisible(*volumeSlider);

    if (ProjectInfo::isStandalone) {
        volumeSlider->onValueChange = [this]() {
            pd->volume->store(volumeSlider->getValue());
        };
    } else {
        volumeAttachment = std::make_unique<SliderParameterAttachment>(*dynamic_cast<RangedAudioParameter*>(pd->getParameters()[0]), *volumeSlider, nullptr);
    }

    addAndMakeVisible(*levelMeter);
    addAndMakeVisible(*midiBlinker);

    levelMeter->toBehind(volumeSlider.get());

    overlayButton.setButtonText(Icons::Eye);
    overlaySettingsButton.setButtonText(Icons::ThinDown);

    overlaySettingsButton.onClick = [this]() {
        auto* editor = dynamic_cast<PluginEditor*>(pd->getActiveEditor());
        OverlayDisplaySettings::show(editor, editor->getLocalArea(this, overlaySettingsButton.getBounds()));
    };

    snapEnableButton.setButtonText(Icons::Magnet);
    snapSettingsButton.setButtonText(Icons::ThinDown);

    snapEnableButton.getToggleStateValue().referTo(SettingsFile::getInstance()->getPropertyAsValue("grid_enabled"));

    snapSettingsButton.onClick = [this]() {
        auto* editor = dynamic_cast<PluginEditor*>(pd->getActiveEditor());
        SnapSettings::show(editor, editor->getLocalArea(this, snapSettingsButton.getBounds()));
    };

    alignmentButton.setButtonText(Icons::AlignLeft);
    alignmentButton.onClick = [this]() {
        auto* editor = dynamic_cast<PluginEditor*>(pd->getActiveEditor());
        AlignmentTools::show(editor, editor->getLocalArea(this, alignmentButton.getBounds()));
    };

    // overlay button
    overlayButton.getProperties().set("Style", "SmallIcon");
    overlaySettingsButton.getProperties().set("Style", "SmallIcon");

    overlayButton.setClickingTogglesState(true);
    overlaySettingsButton.setClickingTogglesState(false);

    addAndMakeVisible(overlayButton);
    addAndMakeVisible(overlaySettingsButton);

    overlayButton.setConnectedEdges(Button::ConnectedOnRight);
    overlaySettingsButton.setConnectedEdges(Button::ConnectedOnLeft);

    overlayButton.getToggleStateValue().referTo(SettingsFile::getInstance()->getValueTree().getChildWithName("Overlays").getPropertyAsValue("alt_mode", nullptr));
    overlayButton.setTooltip(String("Show overlays"));
    overlaySettingsButton.setTooltip(String("Overlay settings"));

    // snapping button
    snapEnableButton.getProperties().set("Style", "SmallIcon");
    snapSettingsButton.getProperties().set("Style", "SmallIcon");

    snapEnableButton.setClickingTogglesState(true);
    snapSettingsButton.setClickingTogglesState(false);

    addAndMakeVisible(snapEnableButton);
    addAndMakeVisible(snapSettingsButton);

    snapEnableButton.setConnectedEdges(Button::ConnectedOnRight);
    snapSettingsButton.setConnectedEdges(Button::ConnectedOnLeft);

    snapEnableButton.setTooltip(String("Enable snapping"));
    snapSettingsButton.setTooltip(String("Snap settings"));

    // alignment button
    alignmentButton.getProperties().set("Style", "SmallIcon");

    addAndMakeVisible(alignmentButton);

    alignmentButton.setTooltip(String("Alignment tools"));

    setSize(getWidth(), statusbarHeight);
}

Statusbar::~Statusbar()
{
    pd->statusbarSource->removeListener(levelMeter.get());
    pd->statusbarSource->removeListener(midiBlinker.get());
    pd->statusbarSource->removeListener(this);
}

void Statusbar::propertyChanged(String const& name, var const& value)
{
}

void Statusbar::paint(Graphics& g)
{
    g.setColour(findColour(PlugDataColour::outlineColourId));
    g.drawLine(0.0f, 0.5f, static_cast<float>(getWidth()), 0.5f);

    g.drawLine(firstSeparatorPosition, 6.0f, firstSeparatorPosition, getHeight() - 6.0f);
    g.drawLine(secondSeparatorPosition, 6.0f, secondSeparatorPosition, getHeight() - 6.0f);
    g.drawLine(thirdSeparatorPosition, 6.0f, thirdSeparatorPosition, getHeight() - 6.0f);
}

void Statusbar::resized()
{
    int pos = 1;
    auto position = [this, &pos](int width, bool inverse = false) -> int {
        int result = 8 + pos;
        pos += width + 3;
        return inverse ? getWidth() - pos : result;
    };

    auto spacing = getHeight() + 4;

    centreButton.setBounds(position(spacing), 0, getHeight(), getHeight());
    fitAllButton.setBounds(position(spacing), 0, getHeight(), getHeight());

    firstSeparatorPosition = position(7) + 3.5f; // Second seperator

    overlayButton.setBounds(position(spacing), 0, getHeight(), getHeight());
    overlaySettingsButton.setBounds(overlayButton.getBounds().translated(getHeight() - 3, 0).withTrimmedRight(8));
    position(10);

    snapEnableButton.setBounds(position(spacing), 0, getHeight(), getHeight());
    snapSettingsButton.setBounds(snapEnableButton.getBounds().translated(getHeight() - 3, 0).withTrimmedRight(8));
    position(10);

    alignmentButton.setBounds(position(spacing), 0, getHeight(), getHeight());

    pos = 4; // reset position for elements on the right

    protectButton.setBounds(position(getHeight(), true), 0, getHeight(), getHeight());

    powerButton.setBounds(position(getHeight(), true), 0, getHeight(), getHeight());

    // TODO: combine these both into one
    int levelMeterPosition = position(110, true);
    levelMeter->setBounds(levelMeterPosition, 2, 120, getHeight() - 4);
    volumeSlider->setBounds(levelMeterPosition, 2, 120, getHeight() - 4);

    secondSeparatorPosition = position(5, true) + 5.0f; // Third seperator

    // Offset to make text look centred
    oversampleSelector->setBounds(position(spacing - 8, true), 1, getHeight() - 2, getHeight() - 2);

    thirdSeparatorPosition = position(5, true) + 2.5f; // Fourth seperator

    midiBlinker->setBounds(position(55, true) - 8, 0, 55, getHeight());
}

void Statusbar::audioProcessedChanged(bool audioProcessed)
{
    auto colour = findColour(audioProcessed ? PlugDataColour::levelMeterActiveColourId : PlugDataColour::signalColourId);

    powerButton.setColour(TextButton::textColourOnId, colour);
}

StatusbarSource::StatusbarSource()
    : numChannels(0)
{
    startTimerHz(30);
}

static bool hasRealEvents(MidiBuffer& buffer)
{
    return std::any_of(buffer.begin(), buffer.end(),
        [](auto const& event) {
            return !event.getMessage().isSysEx();
        });
}

void StatusbarSource::setSampleRate(double const newSampleRate)
{
    sampleRate = static_cast<int>(newSampleRate);
}

void StatusbarSource::setBufferSize(int bufferSize)
{
    this->bufferSize = bufferSize;
}

void StatusbarSource::processBlock(MidiBuffer& midiIn, MidiBuffer& midiOut, int channels)
{
    if (channels == 1) {
        level[1] = 0;
    } else if (channels == 0) {
        level[0] = 0;
        level[1] = 0;
    }

    auto nowInMs = Time::getCurrentTime().getMillisecondCounter();
    auto hasInEvents = hasRealEvents(midiIn);
    auto hasOutEvents = hasRealEvents(midiOut);

    lastAudioProcessedTime = nowInMs;

    if (hasOutEvents)
        lastMidiSentTime = nowInMs;
    if (hasInEvents)
        lastMidiReceivedTime = nowInMs;
}

void StatusbarSource::prepareToPlay(int nChannels)
{
    numChannels = nChannels;
    peakBuffer.reset(sampleRate, bufferSize, nChannels);
}

void StatusbarSource::timerCallback()
{
    auto currentTime = Time::getCurrentTime().getMillisecondCounter();

    auto hasReceivedMidi = currentTime - lastMidiReceivedTime < 700;
    auto hasSentMidi = currentTime - lastMidiSentTime < 700;
    auto hasProcessedAudio = currentTime - lastAudioProcessedTime < 700;

    if (hasReceivedMidi != midiReceivedState) {
        midiReceivedState = hasReceivedMidi;
        for (auto* listener : listeners)
            listener->midiReceivedChanged(hasReceivedMidi);
    }
    if (hasSentMidi != midiSentState) {
        midiSentState = hasSentMidi;
        for (auto* listener : listeners)
            listener->midiSentChanged(hasSentMidi);
    }
    if (hasProcessedAudio != audioProcessedState) {
        audioProcessedState = hasProcessedAudio;
        for (auto* listener : listeners)
            listener->audioProcessedChanged(hasProcessedAudio);
    }

    auto peak = peakBuffer.getPeak();

    for (auto* listener : listeners) {
        listener->audioLevelChanged(peak);
    }
}

void StatusbarSource::addListener(Listener* l)
{
    listeners.push_back(l);
}

void StatusbarSource::removeListener(Listener* l)
{
    listeners.erase(std::remove(listeners.begin(), listeners.end(), l), listeners.end());
}

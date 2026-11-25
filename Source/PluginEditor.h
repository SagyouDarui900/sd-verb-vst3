#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <functional>
#include "PluginProcessor.h"
#include "ui/RetroLookAndFeel.h"
#include "ui/VerbVisualizer.h"

namespace sdverb
{

/**
 * Custom rotary slider that supports:
 *  - Double-click: reset to default value (handled by Slider)
 *  - Shift + Double-click / Alt + Click: inline text entry without resetting value
 *  - Right-click: suppresses accidental knob drag so host context menu opens cleanly
 */
class RetroSlider : public juce::Slider
{
public:
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isAltDown())
        {
            showTextBox();
            return;
        }
        if (e.mods.isPopupMenu())
            return;

        juce::Slider::mouseDown(e);
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (e.mods.isShiftDown())
        {
            showTextBox();
            return;
        }
        juce::Slider::mouseDoubleClick(e);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            return;

        juce::Slider::mouseUp(e);
    }
};

} // namespace sdverb

class SDVerbAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer,
                                   private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit SDVerbAudioProcessorEditor(SDVerbAudioProcessor&);
    ~SDVerbAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct LabeledKnob
    {
        sdverb::RetroSlider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void timerCallback() override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void mouseUp(const juce::MouseEvent& event) override;

    void setupKnob(LabeledKnob& knob, const juce::String& paramId, const juce::String& name,
                   std::function<juce::String(double)> textFormatter = nullptr,
                   std::function<double(const juce::String&)> textParser = nullptr);
    void updateSidebarReadouts();
    void updateVisualizerShape();
    void applyTheme();
    void rebuildSidebarDecorPath();

    SDVerbAudioProcessor& audioProcessor;
    sdverb::RetroLookAndFeel lookAndFeel;
    sdverb::VerbVisualizer visualizer;
    juce::ComboBox presetBox;
    int lastPresetIdx = -1;

    // Theme controls (in Left Sidebar)
    juce::TextButton lightDarkButton { "LIGHT" };
    juce::TextButton colorThemeButton { "ORG" };
    bool isDarkTheme = false;
    bool isCyanTheme = false;

    // Column 1: SPACE CORE (2-segment Mode buttons + 2x2 knobs)
    juce::TextButton denseModeButton { "DENSE" };
    juce::TextButton metalModeButton { "METAL" };
    LabeledKnob sizeKnob, decayKnob, predelayKnob, dampingKnob;

    // Column 2: GATE & DUCKING
    juce::TextButton gateActiveButton { "GATE OFF" };
    juce::TextButton holdSyncButton   { "SYNC: OFF" };
    LabeledKnob gateThreshKnob, gateHoldKnob, gateRateKnob, gateRelKnob, duckingKnob;

    // Column 3: TONE & MIX
    LabeledKnob lowCutKnob, highCutKnob, widthKnob, mixKnob;

    // Button attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gateActiveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> holdSyncAttachment;

    // Cached Y2K honeycomb clusters & geometric vector decor
    juce::Path sidebarDecorStrokePath;
    juce::Path sidebarDecorFillPath;
    juce::Path sidebarDecorSubFillPath;

    // Cached state for sidebar redraw optimization
    int lastGateState = -1;
    float lastDecayVal = -1.0f;
    float lastHoldVal = -1.0f;
    int lastHoldRateIdx = -1;
    bool lastHoldSync = false;
    bool isSilentIdle = false;

    std::atomic<bool> paramsDirty { false };
    std::atomic<bool> isSilentIdleAtomic { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SDVerbAudioProcessorEditor)
};

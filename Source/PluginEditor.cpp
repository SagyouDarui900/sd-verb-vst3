#include "PluginEditor.h"
#include <cmath>

namespace
{
constexpr int kBaseWidth  = 720;
constexpr int kBaseHeight = 410;
constexpr int kVisHeight  = 172;

void addHexagonToPath(juce::Path& path, float cx, float cy, float radius)
{
    constexpr float kDeg60 = 1.0471975511965977f; // pi / 3 (flat-top hexagon)
    for (int i = 0; i < 6; ++i)
    {
        const float angle = static_cast<float>(i) * kDeg60;
        const float px = cx + radius * std::cos(angle);
        const float py = cy + radius * std::sin(angle);
        if (i == 0)
            path.startNewSubPath(px, py);
        else
            path.lineTo(px, py);
    }
    path.closeSubPath();
}

double parseStandardUnitValue(const juce::String& text)
{
    return text.trim().getDoubleValue();
}

double parseFreqUnitValue(const juce::String& text)
{
    const auto trimmed = text.trim();
    const double val = trimmed.getDoubleValue();
    if (trimmed.containsIgnoreCase("k"))
        return val * 1000.0;
    return val;
}
} // namespace

SDVerbAudioProcessorEditor::SDVerbAudioProcessorEditor(SDVerbAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      visualizer(p, lookAndFeel)
{
    setLookAndFeel(&lookAndFeel);

    // Restore saved theme state
    const int savedTheme = juce::jlimit(0, 3, audioProcessor.getSavedThemeId());
    isDarkTheme = (savedTheme & 1) != 0;
    isCyanTheme = (savedTheme & 2) != 0;

    // Aspect-ratio locked resizing (540x308 .. 1440x820)
    setResizable(true, true);
    if (auto* boundsConstrainer = getConstrainer())
    {
        boundsConstrainer->setFixedAspectRatio(static_cast<double>(kBaseWidth) / static_cast<double>(kBaseHeight));
        boundsConstrainer->setMinimumSize(540, 308);
        boundsConstrainer->setMaximumSize(1440, 820);
    }
    setSize(kBaseWidth, kBaseHeight);

    addAndMakeVisible(visualizer);

    // Preset selector in top visualizer bar
    addAndMakeVisible(presetBox);
    for (int i = 0; i < audioProcessor.getNumPrograms(); ++i)
        presetBox.addItem(audioProcessor.getProgramName(i), i + 1);
    lastPresetIdx = audioProcessor.getCurrentProgram();
    presetBox.setSelectedItemIndex(lastPresetIdx, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0)
        {
            lastPresetIdx = idx;
            audioProcessor.setCurrentProgram(idx);
            audioProcessor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withProgramChanged(true));
        }
    };

    // Theme toggle buttons
    addAndMakeVisible(lightDarkButton);
    lightDarkButton.onClick = [this]
    {
        isDarkTheme = !isDarkTheme;
        applyTheme();
    };

    addAndMakeVisible(colorThemeButton);
    colorThemeButton.onClick = [this]
    {
        isCyanTheme = !isCyanTheme;
        applyTheme();
    };

    // Column 1: SPACE CORE (DENSE / METAL segmented toggle)
    addAndMakeVisible(denseModeButton);
    addAndMakeVisible(metalModeButton);
    denseModeButton.onClick = [this]
    {
        if (auto* param = audioProcessor.getAPVTS().getParameter("algo"))
            param->setValueNotifyingHost(0.0f);
    };
    metalModeButton.onClick = [this]
    {
        if (auto* param = audioProcessor.getAPVTS().getParameter("algo"))
            param->setValueNotifyingHost(1.0f);
    };

    setupKnob(sizeKnob, "size", "SIZE",
              [](double v) { return juce::String(v, 1) + " %"; },
              parseStandardUnitValue);
    setupKnob(decayKnob, "decay", "DECAY",
              [](double v) { return juce::String(v, 2) + " s"; },
              parseStandardUnitValue);
    setupKnob(predelayKnob, "predelay", "PRE-DLY",
              [](double v) { return juce::String(v, 1) + " ms"; },
              parseStandardUnitValue);
    setupKnob(predelayRateKnob, "pdly_rate", "PRE-DLY",
              [](double v)
              {
                  static const char* kPdlyRates[6] = { "1/64", "1/32", "1/16T", "1/16", "1/8T", "1/8" };
                  const int idx = juce::jlimit(0, 5, static_cast<int>(std::round(v)));
                  return juce::String(kPdlyRates[idx]);
              },
              [](const juce::String& text) -> double
              {
                  static const char* kPdlyRates[6] = { "1/64", "1/32", "1/16T", "1/16", "1/8T", "1/8" };
                  const auto trimmed = text.trim();
                  for (int i = 0; i < 6; ++i)
                  {
                      if (trimmed.equalsIgnoreCase(kPdlyRates[i]))
                          return static_cast<double>(i);
                  }
                  return trimmed.getDoubleValue();
              });
    setupKnob(dampingKnob, "damping", "DAMPING",
              [](double v) { return juce::String(v, 1) + " %"; },
              parseStandardUnitValue);

    // Column 2: GATE & DUCKING
    addAndMakeVisible(gateActiveButton);
    gateActiveButton.setClickingTogglesState(true);
    gateActiveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), "gate_enabled", gateActiveButton);

    addAndMakeVisible(holdSyncButton);
    holdSyncButton.setClickingTogglesState(true);
    holdSyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), "gate_sync", holdSyncButton);

    setupKnob(gateThreshKnob, "gate_thresh", "THRESH",
              [](double v) { return juce::String(v, 1) + " dB"; },
              parseStandardUnitValue);
    setupKnob(gateHoldKnob, "gate_hold", "HOLD",
              [](double v) { return juce::String(static_cast<int>(std::round(v))) + " ms"; },
              parseStandardUnitValue);
    setupKnob(gateRateKnob, "gate_rate", "HOLD",
              [](double v)
              {
                  static const char* kRates[6] = { "1/32", "1/16", "1/8", "1/4", "1/16T", "1/8D" };
                  const int idx = juce::jlimit(0, 5, static_cast<int>(std::round(v)));
                  return juce::String(kRates[idx]);
              },
              [](const juce::String& text) -> double
              {
                  static const char* kRates[6] = { "1/32", "1/16", "1/8", "1/4", "1/16T", "1/8D" };
                  const auto trimmed = text.trim();
                  for (int i = 0; i < 6; ++i)
                  {
                      if (trimmed.equalsIgnoreCase(kRates[i]))
                          return static_cast<double>(i);
                  }
                  return trimmed.getDoubleValue();
              });
    setupKnob(gateRelKnob, "gate_rel", "RELEASE",
              [](double v) { return juce::String(v, 1) + " ms"; },
              parseStandardUnitValue);
    setupKnob(duckingKnob, "duck_amount", "DUCKING",
              [](double v) { return juce::String(v, 1) + " %"; },
              parseStandardUnitValue);

    // Column 3: TONE & MIX
    addAndMakeVisible(pdlySyncButton);
    pdlySyncButton.setClickingTogglesState(true);
    pdlySyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), "pdly_sync", pdlySyncButton);

    addAndMakeVisible(lofiButton);
    lofiButton.setClickingTogglesState(true);
    lofiAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), "lofi_enabled", lofiButton);

    setupKnob(lowCutKnob, "in_hpf", "LOW CUT",
              [](double v) { return juce::String(static_cast<int>(std::round(v))) + " Hz"; },
              parseFreqUnitValue);
    setupKnob(highCutKnob, "in_lpf", "HIGH CUT",
              [](double v)
              {
                  if (v >= 1000.0)
                      return juce::String(v / 1000.0, 1) + " kHz";
                  return juce::String(static_cast<int>(std::round(v))) + " Hz";
              },
              parseFreqUnitValue);
    setupKnob(widthKnob, "width", "WIDTH",
              [](double v) { return juce::String(static_cast<int>(std::round(v))) + " %"; },
              parseStandardUnitValue);
    setupKnob(mixKnob, "mix", "MIX",
              [](double v) { return juce::String(v, 1) + " %"; },
              parseStandardUnitValue);

    // Listen to parameter changes for Shape Guide & Toggle labels
    for (const auto* id : { "algo", "pdly_sync", "predelay", "pdly_rate", "decay",
                            "gate_enabled", "gate_thresh", "gate_sync", "gate_hold", "gate_rate", "gate_rel",
                            "lofi_enabled", "width" })
    {
        audioProcessor.getAPVTS().addParameterListener(id, this);
    }

    applyTheme();
    updateSidebarReadouts();
    updateVisualizerShape();
    startTimerHz(60);
}

SDVerbAudioProcessorEditor::~SDVerbAudioProcessorEditor()
{
    stopTimer();

    for (auto* sl : { &sizeKnob.slider, &decayKnob.slider, &predelayKnob.slider, &predelayRateKnob.slider, &dampingKnob.slider,
                      &gateThreshKnob.slider, &gateHoldKnob.slider, &gateRateKnob.slider, &gateRelKnob.slider, &duckingKnob.slider,
                      &lowCutKnob.slider, &highCutKnob.slider, &widthKnob.slider, &mixKnob.slider })
    {
        sl->removeMouseListener(this);
    }

    for (const auto* id : { "algo", "pdly_sync", "predelay", "pdly_rate", "decay",
                            "gate_enabled", "gate_thresh", "gate_sync", "gate_hold", "gate_rate", "gate_rel",
                            "lofi_enabled", "width" })
    {
        audioProcessor.getAPVTS().removeParameterListener(id, this);
    }
    setLookAndFeel(nullptr);
}

void SDVerbAudioProcessorEditor::setupKnob(LabeledKnob& knob, const juce::String& paramId, const juce::String& name,
                                           std::function<juce::String(double)> textFormatter,
                                           std::function<double(const juce::String&)> textParser)
{
    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 17);
    knob.slider.setComponentID(paramId);
    knob.slider.addMouseListener(this, true);

    addAndMakeVisible(knob.slider);

    if (auto* param = audioProcessor.getAPVTS().getParameter(paramId))
    {
        const float defNorm = param->getDefaultValue();
        knob.slider.setDoubleClickReturnValue(true, param->convertFrom0to1(defNorm));
    }

    // Create SliderAttachment first, then apply custom text formatters so attachment doesn't overwrite them
    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), paramId, knob.slider);

    if (textFormatter)
        knob.slider.textFromValueFunction = std::move(textFormatter);
    if (textParser)
        knob.slider.valueFromTextFunction = std::move(textParser);

    knob.slider.updateText();

    knob.label.setText(name, juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setFont(lookAndFeel.getNumericFontOptions(11.5f));
    addAndMakeVisible(knob.label);
}

void SDVerbAudioProcessorEditor::mouseUp(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu() && event.eventComponent != nullptr)
    {
        juce::String paramId = event.eventComponent->getComponentID();
        if (paramId.isEmpty())
        {
            if (auto* parentSlider = event.eventComponent->findParentComponentOfClass<sdverb::RetroSlider>())
                paramId = parentSlider->getComponentID();
        }

        if (paramId.isNotEmpty())
        {
            if (auto* param = audioProcessor.getAPVTS().getParameter(paramId))
            {
                if (auto* hostCtx = getHostContext())
                {
                    if (auto menu = hostCtx->getContextMenuForParameter(param))
                        menu->getEquivalentPopupMenu().showMenuAsync(juce::PopupMenu::Options());
                }
            }
        }
    }
}

void SDVerbAudioProcessorEditor::parameterChanged(const juce::String&, float)
{
    paramsDirty.store(true, std::memory_order_release);

    if (isSilentIdleAtomic.load(std::memory_order_acquire))
    {
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<SDVerbAudioProcessorEditor>(this)]
        {
            if (safeThis != nullptr)
            {
                if (safeThis->isSilentIdle)
                {
                    safeThis->isSilentIdle = false;
                    safeThis->isSilentIdleAtomic.store(false, std::memory_order_release);
                    safeThis->startTimerHz(60);
                }
                if (safeThis->paramsDirty.exchange(false, std::memory_order_acq_rel))
                {
                    safeThis->updateSidebarReadouts();
                    safeThis->updateVisualizerShape();
                }
            }
        });
    }
}

void SDVerbAudioProcessorEditor::updateSidebarReadouts()
{
    auto& apvts = audioProcessor.getAPVTS();

    const bool isMetal = apvts.getRawParameterValue("algo")->load() >= 0.5f;
    denseModeButton.setToggleState(!isMetal, juce::dontSendNotification);
    metalModeButton.setToggleState(isMetal, juce::dontSendNotification);

    const bool gateOn = apvts.getRawParameterValue("gate_enabled")->load() >= 0.5f;
    gateActiveButton.setButtonText(gateOn ? "GATE ON" : "GATE OFF");

    const bool syncOn = apvts.getRawParameterValue("gate_sync")->load() >= 0.5f;
    holdSyncButton.setButtonText(syncOn ? "SYNC: ON" : "SYNC: OFF");

    gateHoldKnob.slider.setVisible(!syncOn);
    gateHoldKnob.label.setVisible(!syncOn);
    gateRateKnob.slider.setVisible(syncOn);
    gateRateKnob.label.setVisible(syncOn);

    const bool pdlySyncOn = apvts.getRawParameterValue("pdly_sync")->load() >= 0.5f;
    pdlySyncButton.setButtonText(pdlySyncOn ? "PDLY: SYNC" : "PDLY: OFF");
    predelayKnob.slider.setVisible(!pdlySyncOn);
    predelayKnob.label.setVisible(!pdlySyncOn);
    predelayRateKnob.slider.setVisible(pdlySyncOn);
    predelayRateKnob.label.setVisible(pdlySyncOn);

    const bool lofiOn = apvts.getRawParameterValue("lofi_enabled")->load() >= 0.5f;
    lofiButton.setButtonText(lofiOn ? "LO-FI: ON" : "LO-FI: OFF");

    lastDecayVal    = apvts.getRawParameterValue("decay")->load();
    lastHoldVal     = apvts.getRawParameterValue("gate_hold")->load();
    lastHoldRateIdx = static_cast<int>(apvts.getRawParameterValue("gate_rate")->load());
    lastHoldSync    = syncOn;

    repaint();
}

void SDVerbAudioProcessorEditor::updateVisualizerShape()
{
    auto& apvts = audioProcessor.getAPVTS();
    const bool  pdlySyncOn = apvts.getRawParameterValue("pdly_sync")->load() >= 0.5f;
    float predelay         = apvts.getRawParameterValue("predelay")->load();
    const float decay      = apvts.getRawParameterValue("decay")->load();
    const bool  gateOn     = apvts.getRawParameterValue("gate_enabled")->load() >= 0.5f;
    const float gateThresh = apvts.getRawParameterValue("gate_thresh")->load();
    const bool  syncOn     = apvts.getRawParameterValue("gate_sync")->load() >= 0.5f;
    float holdMs           = apvts.getRawParameterValue("gate_hold")->load();
    const float gateRel    = apvts.getRawParameterValue("gate_rel")->load();
    const float width      = apvts.getRawParameterValue("width")->load();

    const double bpm = audioProcessor.getLastKnownBpm();
    if (pdlySyncOn)
    {
        const int rateIdx = static_cast<int>(apvts.getRawParameterValue("pdly_rate")->load());
        constexpr float pdlyBeatsTable[6] = { 0.0625f, 0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.5f };
        predelay = juce::jlimit(0.0f, 500.0f, static_cast<float>((60.0 / bpm) * pdlyBeatsTable[juce::jlimit(0, 5, rateIdx)] * 1000.0));
    }

    if (syncOn)
    {
        const int rateIdx = static_cast<int>(apvts.getRawParameterValue("gate_rate")->load());
        constexpr float beatsTable[6] = { 0.125f, 0.25f, 0.5f, 1.0f, 1.0f / 6.0f, 0.75f };
        holdMs = static_cast<float>((60.0 / bpm) * beatsTable[juce::jlimit(0, 5, rateIdx)] * 1000.0);
    }

    visualizer.updateShapeParameters(predelay, decay, gateOn, gateThresh, holdMs, gateRel, width);
}

void SDVerbAudioProcessorEditor::applyTheme()
{
    int idx = 0;
    if (isDarkTheme) idx += 1;
    if (isCyanTheme) idx += 2;

    audioProcessor.setSavedThemeId(idx);
    lookAndFeel.setTheme(static_cast<sdverb::ThemeId>(idx));
    lightDarkButton.setButtonText(isDarkTheme ? "DARK" : "LIGHT");
    colorThemeButton.setButtonText(isCyanTheme ? "CYN" : "ORG");

    const auto& pal = lookAndFeel.getCurrentPalette();
    for (auto* lbl : { &sizeKnob.label, &decayKnob.label, &predelayKnob.label, &predelayRateKnob.label, &dampingKnob.label,
                       &gateThreshKnob.label, &gateHoldKnob.label, &gateRateKnob.label, &gateRelKnob.label, &duckingKnob.label,
                       &lowCutKnob.label, &highCutKnob.label, &widthKnob.label, &mixKnob.label })
    {
        lbl->setColour(juce::Label::textColourId, pal.textMain);
    }

    for (auto* sl : { &sizeKnob.slider, &decayKnob.slider, &predelayKnob.slider, &predelayRateKnob.slider, &dampingKnob.slider,
                      &gateThreshKnob.slider, &gateHoldKnob.slider, &gateRateKnob.slider, &gateRelKnob.slider, &duckingKnob.slider,
                      &lowCutKnob.slider, &highCutKnob.slider, &widthKnob.slider, &mixKnob.slider })
    {
        sl->setColour(juce::Slider::textBoxTextColourId, pal.textMain);
        sl->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    sendLookAndFeelChange();
    presetBox.setColour(juce::ComboBox::textColourId, pal.textMain);
    presetBox.setColour(juce::ComboBox::backgroundColourId, pal.baseBg);
    presetBox.setColour(juce::ComboBox::outlineColourId, pal.mainAccent);
    repaint();
}

void SDVerbAudioProcessorEditor::timerCallback()
{
    // Sync preset selector if host changed current program externally
    const int curProg = audioProcessor.getCurrentProgram();
    if (curProg != lastPresetIdx)
    {
        lastPresetIdx = curProg;
        presetBox.setSelectedItemIndex(curProg, juce::dontSendNotification);
    }

    // Sync theme if processor state changed externally (e.g. preset load)
    const int curIdx = (isDarkTheme ? 1 : 0) + (isCyanTheme ? 2 : 0);
    const int savedIdx = juce::jlimit(0, 3, audioProcessor.getSavedThemeId());
    if (savedIdx != curIdx)
    {
        isDarkTheme = (savedIdx & 1) != 0;
        isCyanTheme = (savedIdx & 2) != 0;
        applyTheme();
    }

    if (paramsDirty.exchange(false, std::memory_order_acq_rel))
    {
        updateSidebarReadouts();
        updateVisualizerShape();
    }

    const bool silent = audioProcessor.isAudioSilent();
    if (silent)
    {
        if (!isSilentIdle)
        {
            isSilentIdle = true;
            isSilentIdleAtomic.store(true, std::memory_order_release);
            startTimerHz(2);
        }
        return;
    }

    if (isSilentIdle)
    {
        isSilentIdle = false;
        isSilentIdleAtomic.store(false, std::memory_order_release);
        startTimerHz(60);
    }

    visualizer.pushNewAudioData();

    const int gateState = audioProcessor.getGateStateAtomic();
    if (gateState != lastGateState)
    {
        lastGateState = gateState;
        repaint();
    }
}

void SDVerbAudioProcessorEditor::rebuildSidebarDecorPath()
{
    sidebarDecorStrokePath.clear();
    sidebarDecorFillPath.clear();
    sidebarDecorSubFillPath.clear();

    const float scaleX = static_cast<float>(getWidth()) / static_cast<float>(kBaseWidth);
    const float scaleY = static_cast<float>(getHeight()) / static_cast<float>(kBaseHeight);
    const float scale  = juce::jmin(scaleX, scaleY);

    const float sbW = static_cast<float>(getWidth()) * (164.0f / static_cast<float>(kBaseWidth));

    // --- 1. Upper-right accent honeycomb + 45-deg parallel lines (clear of SD-Verb text) ---
    {
        const float rTop = 13.5f * scale;
        const float cx1 = sbW - 14.0f * scaleX;
        const float cy1 = 22.0f * scaleY;
        addHexagonToPath(sidebarDecorSubFillPath, cx1, cy1, rTop * 0.72f);
        addHexagonToPath(sidebarDecorStrokePath, cx1, cy1, rTop);
        addHexagonToPath(sidebarDecorStrokePath, cx1 - rTop * 1.55f, cy1 + rTop * 0.88f, rTop * 0.56f);

        // 2 parallel 45-degree circuit lines routing down from top cluster
        for (int k = 0; k < 2; ++k)
        {
            const float off = static_cast<float>(k) * 4.0f * scale;
            sidebarDecorStrokePath.startNewSubPath(cx1 - 36.0f * scaleX, 6.0f * scaleY + off);
            sidebarDecorStrokePath.lineTo(sbW - 22.0f * scaleX - off, 6.0f * scaleY + off);
            sidebarDecorStrokePath.lineTo(sbW - 8.0f * scaleX - off, 20.0f * scaleY + off * 1.6f);
            sidebarDecorStrokePath.lineTo(sbW - 8.0f * scaleX - off, 125.0f * scaleY);
        }
    }

    // --- 2. Main Y2K Honeycomb Cluster (y = 240..360 space below GATE badge) ---
    struct HexSpec { float x; float y; float r; int style; }; // 0: double-stroke, 1: solid white, 2: solid subDecor + stroke
    const HexSpec cluster[] = {
        {  38.0f, 276.0f, 20.0f, 0 },
        {  72.0f, 295.0f, 20.0f, 1 },
        {  38.0f, 315.0f, 19.0f, 2 },
        { 104.0f, 275.0f, 15.0f, 0 },
        { 105.0f, 313.0f, 16.0f, 0 },
        {  72.0f, 334.0f, 15.0f, 1 },
        {  14.0f, 297.0f, 12.0f, 1 },
        {  16.0f, 340.0f, 11.0f, 0 },
        { 128.0f, 338.0f, 12.0f, 2 },
        { 132.0f, 250.0f, 10.0f, 0 }
    };

    for (const auto& h : cluster)
    {
        const float cx = h.x * scaleX;
        const float cy = h.y * scaleY;
        const float r  = h.r * scale;

        if (h.style == 1)
        {
            addHexagonToPath(sidebarDecorFillPath, cx, cy, r * 0.82f);
        }
        else if (h.style == 2)
        {
            addHexagonToPath(sidebarDecorSubFillPath, cx, cy, r * 0.78f);
            addHexagonToPath(sidebarDecorStrokePath, cx, cy, r);
        }
        else
        {
            addHexagonToPath(sidebarDecorStrokePath, cx, cy, r);
            addHexagonToPath(sidebarDecorStrokePath, cx, cy, r * 0.64f);
        }
    }

    // --- 3. 2 parallel 45-degree bent vector lines around lower cluster ---
    for (int k = 0; k < 2; ++k)
    {
        const float off = static_cast<float>(k) * 4.0f * scale;
        sidebarDecorStrokePath.startNewSubPath(0.0f, 246.0f * scaleY + off);
        sidebarDecorStrokePath.lineTo(28.0f * scaleX - off * 0.5f, 246.0f * scaleY + off);
        sidebarDecorStrokePath.lineTo(48.0f * scaleX - off * 0.5f, 266.0f * scaleY + off);
        sidebarDecorStrokePath.lineTo(118.0f * scaleX, 266.0f * scaleY + off);
        sidebarDecorStrokePath.lineTo(138.0f * scaleX + off * 0.5f, 246.0f * scaleY + off);
        sidebarDecorStrokePath.lineTo(sbW - 6.0f * scaleX, 246.0f * scaleY + off);

        // Lower framing bracket
        sidebarDecorStrokePath.startNewSubPath(0.0f, 355.0f * scaleY - off);
        sidebarDecorStrokePath.lineTo(86.0f * scaleX, 355.0f * scaleY - off);
        sidebarDecorStrokePath.lineTo(106.0f * scaleX + off * 0.5f, 335.0f * scaleY - off);
        sidebarDecorStrokePath.lineTo(sbW - 6.0f * scaleX, 335.0f * scaleY - off);
    }
}

void SDVerbAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto& pal = lookAndFeel.getCurrentPalette();
    const float scaleX = static_cast<float>(getWidth()) / static_cast<float>(kBaseWidth);
    const float scaleY = static_cast<float>(getHeight()) / static_cast<float>(kBaseHeight);
    const float scale  = juce::jmin(scaleX, scaleY);

    const float sidebarW = 164.0f * scaleX;
    const float visH     = static_cast<float>(kVisHeight) * scaleY;

    // 1. Main background (solid opaque baseBg)
    g.fillAll(pal.baseBg);

    // 2. Left Sidebar (solid opaque sidebarBg)
    const juce::Rectangle<float> sidebarRect(0.0f, 0.0f, sidebarW, static_cast<float>(getHeight()));
    g.setColour(pal.sidebarBg);
    g.fillRect(sidebarRect);

    // Cached Y2K honeycomb clusters & parallel lines inside sidebar (all opaque)
    {
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(sidebarRect.toNearestInt());

        g.setColour(pal.subDecor);
        g.fillPath(sidebarDecorSubFillPath);

        g.setColour(pal.textInverted);
        g.fillPath(sidebarDecorFillPath);
        g.strokePath(sidebarDecorStrokePath, juce::PathStrokeType(juce::jmax(1.1f, 1.35f * scale)));

        // 4 vertical circular dot rings (o o o o) at right side of lower cluster
        const float dotCx = sidebarW - 14.0f * scaleX;
        const float dotStartY = 264.0f * scaleY;
        const float dotDia = 6.0f * scale;
        const float dotStep = 10.5f * scaleY;
        for (int i = 0; i < 4; ++i)
        {
            const float cy = dotStartY + static_cast<float>(i) * dotStep;
            if (i == 0)
                g.fillEllipse(dotCx - dotDia * 0.5f, cy - dotDia * 0.5f, dotDia, dotDia);
            else
                g.drawEllipse(dotCx - dotDia * 0.5f, cy - dotDia * 0.5f, dotDia, dotDia, 1.4f * scale);
        }
    }

    // Solid 2px pure-white (dark: #1E1F22) divider line inside right edge of sidebar + crisp outer edge
    g.setColour(pal.baseBg);
    g.fillRect(sidebarW - 4.0f * scaleX, 0.0f, 2.0f, static_cast<float>(getHeight()));
    g.setColour(pal.textMain);
    g.fillRect(sidebarW - 1.5f, 0.0f, 1.5f, static_cast<float>(getHeight()));

    // Sidebar text: Title & Spec micro-label
    g.setColour(pal.textInverted);
    g.setFont(lookAndFeel.getHeavyFontOptions(24.0f * scaleY));
    g.drawFittedText("SD-Verb", juce::Rectangle<int>(juce::roundToInt(12.0f * scaleX), juce::roundToInt(11.0f * scaleY),
                                                     juce::roundToInt(sidebarW - 30.0f * scaleX), juce::roundToInt(30.0f * scaleY)),
                     juce::Justification::centredLeft, 1);

    g.setFont(lookAndFeel.getNumericFontOptions(10.8f * scaleY));
    g.drawFittedText("SHORT & GATE", juce::Rectangle<int>(juce::roundToInt(12.0f * scaleX), juce::roundToInt(41.0f * scaleY),
                                                          juce::roundToInt(sidebarW - 24.0f * scaleX), juce::roundToInt(14.0f * scaleY)),
                     juce::Justification::centredLeft, 1);

    // Micro spec graphic line below title
    g.setColour(pal.headerBandBg);
    g.fillRect(12.0f * scaleX, 57.0f * scaleY, sidebarW - 26.0f * scaleX, 13.0f * scaleY);
    g.setColour(pal.textInverted);
    g.setFont(lookAndFeel.getNumericFontOptions(8.0f * scaleY));
    g.drawFittedText("8CH FDN // ZERO LATENCY // VST3",
                     juce::Rectangle<int>(juce::roundToInt(15.0f * scaleX), juce::roundToInt(57.0f * scaleY),
                                          juce::roundToInt(sidebarW - 32.0f * scaleX), juce::roundToInt(13.0f * scaleY)),
                     juce::Justification::centredLeft, 1, 0.80f);

    // Sidebar readouts: DECAY & HOLD
    g.setColour(pal.textInverted);
    g.setFont(lookAndFeel.getNumericFontOptions(11.5f * scaleY));
    g.drawText("DECAY", juce::Rectangle<float>(12.0f * scaleX, 82.0f * scaleY, sidebarW - 24.0f * scaleX, 15.0f * scaleY),
               juce::Justification::centredLeft, false);

    g.setFont(lookAndFeel.getHeavyFontOptions(22.5f * scaleY));
    g.drawFittedText(juce::String(lastDecayVal, 2) + " s",
                     juce::Rectangle<int>(juce::roundToInt(12.0f * scaleX), juce::roundToInt(97.0f * scaleY),
                                          juce::roundToInt(sidebarW - 24.0f * scaleX), juce::roundToInt(27.0f * scaleY)),
                     juce::Justification::centredLeft, 1);

    g.setFont(lookAndFeel.getNumericFontOptions(11.5f * scaleY));
    g.drawText("HOLD", juce::Rectangle<float>(12.0f * scaleX, 136.0f * scaleY, sidebarW - 24.0f * scaleX, 15.0f * scaleY),
               juce::Justification::centredLeft, false);

    static const char* kRateNames[6] = { "1/32", "1/16", "1/8", "1/4", "1/16T", "1/8D" };
    const juce::String holdText = lastHoldSync
        ? juce::String(kRateNames[juce::jlimit(0, 5, lastHoldRateIdx)])
        : (juce::String(static_cast<int>(std::round(lastHoldVal))) + " ms");

    g.setFont(lookAndFeel.getHeavyFontOptions(22.5f * scaleY));
    g.drawFittedText(holdText,
                     juce::Rectangle<int>(juce::roundToInt(12.0f * scaleX), juce::roundToInt(151.0f * scaleY),
                                          juce::roundToInt(sidebarW - 24.0f * scaleX), juce::roundToInt(27.0f * scaleY)),
                     juce::Justification::centredLeft, 1);

    // 3-State Gate Indicator Badge (opaque high contrast)
    const auto badgeRect = juce::Rectangle<float>(12.0f * scaleX, 198.0f * scaleY, sidebarW - 26.0f * scaleX, 26.0f * scaleY);
    const float badgeCorner = badgeRect.getHeight() * 0.5f;
    const auto gState = static_cast<sdverb::GateDucker::GateState>(lastGateState < 0 ? 0 : lastGateState);

    juce::String badgeText = "GATE: BYPASS";
    if (gState == sdverb::GateDucker::GateState::Open)
    {
        badgeText = "GATE: OPEN";
        g.setColour(pal.textInverted);
        g.fillRoundedRectangle(badgeRect, badgeCorner);
        g.setColour(pal.sidebarBg);
    }
    else if (gState == sdverb::GateDucker::GateState::Hold)
    {
        badgeText = "GATE: HOLD";
        g.setColour(pal.sidebarBg);
        g.fillRoundedRectangle(badgeRect, badgeCorner);
        g.setColour(pal.textInverted);
        g.drawRoundedRectangle(badgeRect, badgeCorner, 2.0f);
    }
    else if (gState == sdverb::GateDucker::GateState::Shut)
    {
        badgeText = "GATE: SHUT";
        g.setColour(pal.subDecor);
        g.fillRoundedRectangle(badgeRect, badgeCorner);
        g.setColour(pal.textMain);
        g.drawRoundedRectangle(badgeRect, badgeCorner, 1.5f);
    }
    else
    {
        // BYPASS: opaque dark band fill + pure white text
        g.setColour(pal.headerBandBg);
        g.fillRoundedRectangle(badgeRect, badgeCorner);
        g.setColour(pal.textInverted);
    }

    g.setFont(lookAndFeel.getHeavyFontOptions(10.8f * scaleY));
    g.drawFittedText(badgeText, badgeRect.reduced(5.0f * scaleX, 0.0f).toNearestInt(), juce::Justification::centred, 1);

    // 3. Area Divider Lines (solid opaque 1.5px)
    g.setColour(pal.textMain);
    g.fillRect(sidebarW, visH - 1.0f, static_cast<float>(getWidth()) - sidebarW, 1.5f);

    const float ctrlX = sidebarW;
    const float ctrlW = static_cast<float>(getWidth()) - sidebarW;
    const float colW  = ctrlW / 3.0f;

    g.fillRect(ctrlX + colW - 0.75f, visH, 1.5f, static_cast<float>(getHeight()) - visH);
    g.fillRect(ctrlX + colW * 2.0f - 0.75f, visH, 1.5f, static_cast<float>(getHeight()) - visH);

    // 4. Y2K Solid-band Section Headers (height 18px scaled + white heavy text)
    const float bandH = 18.0f * scaleY;
    const float bandY = visH + 3.0f * scaleY;
    const char* colTitles[3] = { "1. SPACE CORE", "2. GATE & DUCKING", "3. TONE & MIX" };
    for (int i = 0; i < 3; ++i)
    {
        const auto bandRect = juce::Rectangle<float>(ctrlX + colW * static_cast<float>(i) + 4.0f * scaleX,
                                                     bandY,
                                                     colW - 8.0f * scaleX,
                                                     bandH);
        g.setColour(pal.headerBandBg);
        g.fillRect(bandRect);

        g.setColour(pal.textInverted);
        g.setFont(lookAndFeel.getHeavyFontOptions(10.2f * scaleY));
        g.drawFittedText(colTitles[i], bandRect.reduced(6.0f * scaleX, 0.0f).toNearestInt(),
                         juce::Justification::centredLeft, 1, 0.80f);
    }
}

void SDVerbAudioProcessorEditor::resized()
{
    rebuildSidebarDecorPath();

    const float scaleX = static_cast<float>(getWidth()) / static_cast<float>(kBaseWidth);
    const float scaleY = static_cast<float>(getHeight()) / static_cast<float>(kBaseHeight);

    const int sidebarW = juce::roundToInt(164.0f * scaleX);
    const int visH     = juce::roundToInt(static_cast<float>(kVisHeight) * scaleY);

    // Top-right Visualizer & Preset Selector (right side of scope header bar)
    visualizer.setBounds(sidebarW, 0, getWidth() - sidebarW, visH);
    {
        const float visW = static_cast<float>(getWidth() - sidebarW);
        const float visHf = static_cast<float>(visH);
        const float guideW = juce::jmin(visW * 0.30f, visHf * 0.92f);
        const int scopeRight = sidebarW + juce::roundToInt(visW - guideW - 2.0f) - 6;
        const int scopeBadgeEnd = sidebarW + 6 + juce::roundToInt(juce::jmin(182.0f, visW - guideW - 14.0f));
        const int comboW = juce::jlimit(110, 194, scopeRight - scopeBadgeEnd - 6);
        const int comboH = juce::roundToInt(18.0f * scaleY);
        presetBox.setBounds(scopeRight - comboW, juce::roundToInt(5.5f * scaleY), comboW, comboH);
    }

    // Sidebar Theme buttons at bottom
    const int btnW = juce::roundToInt(64.0f * scaleX);
    const int btnH = juce::roundToInt(24.0f * scaleY);
    const int btnY = getHeight() - btnH - juce::roundToInt(15.0f * scaleY);
    lightDarkButton.setBounds(juce::roundToInt(12.0f * scaleX), btnY, btnW, btnH);
    colorThemeButton.setBounds(sidebarW - btnW - juce::roundToInt(12.0f * scaleX), btnY, btnW, btnH);

    // Bottom-right 3 Columns with generous bottom margin (12px scaled)
    const int ctrlX = sidebarW;
    const int ctrlY = visH + juce::roundToInt(25.0f * scaleY);
    const int ctrlW = getWidth() - sidebarW;
    const int ctrlH = getHeight() - ctrlY - juce::roundToInt(12.0f * scaleY);
    const int colW  = ctrlW / 3;

    auto placeKnob = [this, scaleY](LabeledKnob& k, juce::Rectangle<int> cell)
    {
        const int lblH = juce::roundToInt(14.0f * scaleY);
        k.label.setFont(lookAndFeel.getNumericFontOptions(11.5f * scaleY));
        k.label.setBounds(cell.removeFromTop(lblH));
        k.slider.setBounds(cell.reduced(0, juce::roundToInt(1.0f * scaleY)));
    };

    // Column 1: SPACE CORE (2-segment Mode buttons + 2x2 knobs)
    {
        auto col = juce::Rectangle<int>(ctrlX, ctrlY, colW, ctrlH).reduced(juce::roundToInt(4.0f * scaleX), 0);
        auto topBar = col.removeFromTop(juce::roundToInt(24.0f * scaleY));
        const int halfBtnW = topBar.getWidth() / 2;
        denseModeButton.setBounds(topBar.removeFromLeft(halfBtnW).reduced(2, 0));
        metalModeButton.setBounds(topBar.reduced(2, 0));
        col.removeFromTop(juce::roundToInt(4.0f * scaleY));

        const int rowH = col.getHeight() / 2;
        const int halfW = col.getWidth() / 2;
        auto row1 = col.removeFromTop(rowH);
        placeKnob(sizeKnob,         row1.removeFromLeft(halfW));
        placeKnob(decayKnob,        row1);
        auto pdlyCell = col.removeFromLeft(halfW);
        placeKnob(predelayKnob,     pdlyCell);
        placeKnob(predelayRateKnob, pdlyCell);
        placeKnob(dampingKnob,      col);
    }

    // Column 2: GATE & DUCKING (2 toggle buttons + 2x2 knobs)
    {
        auto col = juce::Rectangle<int>(ctrlX + colW, ctrlY, colW, ctrlH).reduced(juce::roundToInt(4.0f * scaleX), 0);
        auto topBar = col.removeFromTop(juce::roundToInt(24.0f * scaleY));
        const int halfBtnW = topBar.getWidth() / 2;
        gateActiveButton.setBounds(topBar.removeFromLeft(halfBtnW).reduced(2, 0));
        holdSyncButton.setBounds(topBar.reduced(2, 0));
        col.removeFromTop(juce::roundToInt(4.0f * scaleY));

        const int rowH = col.getHeight() / 2;
        const int halfW = col.getWidth() / 2;
        auto row1 = col.removeFromTop(rowH);
        placeKnob(gateThreshKnob, row1.removeFromLeft(halfW));
        placeKnob(gateHoldKnob,   row1);
        placeKnob(gateRateKnob,   row1);
        placeKnob(gateRelKnob,    col.removeFromLeft(halfW));
        placeKnob(duckingKnob,    col);
    }

    // Column 3: TONE & MIX (2 toggle buttons + 2x2 knobs aligned with other columns)
    {
        auto col = juce::Rectangle<int>(ctrlX + colW * 2, ctrlY, colW, ctrlH).reduced(juce::roundToInt(4.0f * scaleX), 0);
        auto topBar = col.removeFromTop(juce::roundToInt(24.0f * scaleY));
        const int halfBtnW = topBar.getWidth() / 2;
        pdlySyncButton.setBounds(topBar.removeFromLeft(halfBtnW).reduced(2, 0));
        lofiButton.setBounds(topBar.reduced(2, 0));
        col.removeFromTop(juce::roundToInt(4.0f * scaleY));

        const int rowH = col.getHeight() / 2;
        const int halfW = col.getWidth() / 2;
        auto row1 = col.removeFromTop(rowH);
        placeKnob(lowCutKnob,  row1.removeFromLeft(halfW));
        placeKnob(highCutKnob, row1);
        placeKnob(widthKnob,   col.removeFromLeft(halfW));
        placeKnob(mixKnob,     col);
    }
}

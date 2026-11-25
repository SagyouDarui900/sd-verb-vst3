#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
struct FactoryPreset
{
    const char* name;
    int algo;           // 0 = DENSE, 1 = METAL
    float size;         // 10..100 %
    float decay;        // 0.05..5.0 s
    bool pdlySync;
    float predelay;     // 0..100 ms
    int pdlyRate;       // 0..5 (1/64, 1/32, 1/16T, 1/16, 1/8T, 1/8)
    float damping;      // 0..100 %
    float inHpf;        // 20..1000 Hz
    float inLpf;        // 1000..20000 Hz
    bool gateEnabled;
    float gateThresh;   // -60..0 dB
    bool gateSync;
    float gateHold;     // 0..500 ms
    int gateRate;       // 0..5 (1/32, 1/16, 1/8, 1/4, 1/16T, 1/8D)
    float gateRel;      // 5..300 ms
    float duckAmount;   // 0..100 %
    bool lofiEnabled;
    float width;        // 0..200 %
    float mix;          // 0..100 %
};

constexpr FactoryPreset kFactoryPresets[] = {
    { "Default (Init)",                 0, 35.0f, 0.80f, false, 10.0f, 1, 40.0f, 120.0f, 12000.0f, false, -24.0f, false,  80.0f, 1,  30.0f,  0.0f, false, 100.0f, 30.0f },
    { "Main Lead - Tight Ducking",      0, 38.0f, 0.75f, false, 12.0f, 1, 35.0f, 220.0f, 11000.0f, false, -24.0f, false,  80.0f, 1,  30.0f, 55.0f, false, 115.0f, 28.0f },
    { "Main Lead - 1/16 Gate",          0, 42.0f, 1.10f, false,  0.0f, 1, 30.0f, 250.0f, 12000.0f, true,  -28.0f, true,   80.0f, 1,  25.0f, 20.0f, false, 120.0f, 32.0f },
    { "Harmony / Backing - Wide",       0, 55.0f, 1.15f, false, 18.0f, 1, 55.0f, 300.0f,  8500.0f, false, -24.0f, false,  80.0f, 1,  30.0f, 30.0f, false, 165.0f, 35.0f },
    { "Vocal - Short Room + Duck",      0, 40.0f, 0.90f, false, 20.0f, 1, 45.0f, 180.0f,  9500.0f, false, -24.0f, false,  80.0f, 1,  30.0f, 45.0f, false, 110.0f, 26.0f },
    { "Drum Snare - Gated Metal",       1, 52.0f, 1.60f, false,  5.0f, 1, 20.0f, 200.0f, 14000.0f, true,  -26.0f, false, 120.0f, 1,  35.0f,  0.0f, false, 135.0f, 38.0f },
    { "Drum Kit - Tight Room",          0, 22.0f, 0.38f, false,  4.0f, 1, 50.0f, 160.0f,  9000.0f, false, -24.0f, false,  80.0f, 1,  30.0f, 15.0f, false,  90.0f, 20.0f },
    { "SE / Synth Hit - Metallic Tail", 1, 78.0f, 2.40f, false, 15.0f, 1, 15.0f, 140.0f, 16000.0f, false, -24.0f, false,  80.0f, 1,  30.0f,  0.0f, false, 150.0f, 40.0f },
    { "Main Lead - 1/16 Sync Pre-Delay",0, 36.0f, 0.65f, true,  10.0f, 3, 35.0f, 220.0f, 12000.0f, false, -24.0f, false,  80.0f, 1,  30.0f, 40.0f, false, 120.0f, 32.0f },
    { "SE / Retro - Lo-Fi Metal",       1, 65.0f, 1.80f, false,  8.0f, 1, 30.0f, 240.0f, 10000.0f, true,  -28.0f, true,   80.0f, 2,  35.0f,  0.0f, true,  140.0f, 38.0f }
};

constexpr int kNumFactoryPresets = static_cast<int>(sizeof(kFactoryPresets) / sizeof(kFactoryPresets[0]));
} // namespace

SDVerbAudioProcessor::SDVerbAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    algoParam        = apvts.getRawParameterValue("algo");
    sizeParam        = apvts.getRawParameterValue("size");
    decayParam       = apvts.getRawParameterValue("decay");
    pdlySyncParam    = apvts.getRawParameterValue("pdly_sync");
    predelayParam    = apvts.getRawParameterValue("predelay");
    pdlyRateParam    = apvts.getRawParameterValue("pdly_rate");
    dampingParam     = apvts.getRawParameterValue("damping");
    inHpfParam       = apvts.getRawParameterValue("in_hpf");
    inLpfParam       = apvts.getRawParameterValue("in_lpf");
    gateEnabledParam = apvts.getRawParameterValue("gate_enabled");
    gateThreshParam  = apvts.getRawParameterValue("gate_thresh");
    gateSyncParam    = apvts.getRawParameterValue("gate_sync");
    gateHoldParam    = apvts.getRawParameterValue("gate_hold");
    gateRateParam    = apvts.getRawParameterValue("gate_rate");
    gateRelParam     = apvts.getRawParameterValue("gate_rel");
    duckAmountParam  = apvts.getRawParameterValue("duck_amount");
    lofiEnabledParam = apvts.getRawParameterValue("lofi_enabled");
    widthParam       = apvts.getRawParameterValue("width");
    mixParam         = apvts.getRawParameterValue("mix");
}

juce::AudioProcessorValueTreeState::ParameterLayout SDVerbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("algo", 1), "Mode",
        juce::StringArray { "DENSE", "METAL" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("size", 1), "Size",
        juce::NormalisableRange<float>(10.0f, 100.0f, 0.1f), 35.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("decay", 1), "Decay",
        juce::NormalisableRange<float>(0.05f, 5.0f, 0.01f, 0.45f), 0.80f,
        juce::AudioParameterFloatAttributes().withLabel("s")));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("pdly_sync", 1), "Pre-Delay Sync", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("predelay", 1), "Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 10.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("pdly_rate", 1), "Pre-Delay Rate",
        juce::StringArray { "1/64", "1/32", "1/16T", "1/16", "1/8T", "1/8" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("damping", 1), "Damping",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 40.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("in_hpf", 1), "Low Cut",
        juce::NormalisableRange<float>(20.0f, 1000.0f, 1.0f, 0.35f), 120.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("in_lpf", 1), "High Cut",
        juce::NormalisableRange<float>(1000.0f, 20000.0f, 1.0f, 0.35f), 12000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("gate_enabled", 1), "Gate Active", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("gate_thresh", 1), "Gate Thresh",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -24.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("gate_sync", 1), "Hold Sync", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("gate_hold", 1), "Hold Time",
        juce::NormalisableRange<float>(0.0f, 500.0f, 0.5f), 80.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("gate_rate", 1), "Hold Rate",
        juce::StringArray { "1/32", "1/16", "1/8", "1/4", "1/16T", "1/8D" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("gate_rel", 1), "Gate Rel",
        juce::NormalisableRange<float>(5.0f, 300.0f, 0.5f, 0.6f), 30.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("duck_amount", 1), "Ducking",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("lofi_enabled", 1), "Lo-Fi Feedback", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("width", 1), "Width",
        juce::NormalisableRange<float>(0.0f, 200.0f, 0.5f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("mix", 1), "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 30.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    return { params.begin(), params.end() };
}

void SDVerbAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    const double safeSr = (sampleRate > 0.0) ? sampleRate : 44100.0;

    reverbEngine.prepare(safeSr);
    gateDucker.prepare(safeSr);
    scopeFifo.reset();

    // 5.0 ms smoothing for all continuous parameters
    constexpr double smoothTimeSec = 0.005;
    sizeSmooth.reset(safeSr, smoothTimeSec);
    decaySmooth.reset(safeSr, smoothTimeSec);
    predelaySmooth.reset(safeSr, smoothTimeSec);
    dampingSmooth.reset(safeSr, smoothTimeSec);
    inHpfSmooth.reset(safeSr, smoothTimeSec);
    inLpfSmooth.reset(safeSr, smoothTimeSec);
    gateThreshSmooth.reset(safeSr, smoothTimeSec);
    gateHoldSmooth.reset(safeSr, smoothTimeSec);
    gateRelSmooth.reset(safeSr, smoothTimeSec);
    duckAmountSmooth.reset(safeSr, smoothTimeSec);
    widthSmooth.reset(safeSr, smoothTimeSec);
    mixSmooth.reset(safeSr, smoothTimeSec);

    sizeSmooth.setCurrentAndTargetValue(sizeParam->load());
    decaySmooth.setCurrentAndTargetValue(decayParam->load());
    predelaySmooth.setCurrentAndTargetValue(predelayParam->load());
    dampingSmooth.setCurrentAndTargetValue(dampingParam->load());
    inHpfSmooth.setCurrentAndTargetValue(inHpfParam->load());
    inLpfSmooth.setCurrentAndTargetValue(inLpfParam->load());
    gateThreshSmooth.setCurrentAndTargetValue(gateThreshParam->load());
    gateHoldSmooth.setCurrentAndTargetValue(gateHoldParam->load());
    gateRelSmooth.setCurrentAndTargetValue(gateRelParam->load());
    duckAmountSmooth.setCurrentAndTargetValue(duckAmountParam->load());
    widthSmooth.setCurrentAndTargetValue(widthParam->load());
    mixSmooth.setCurrentAndTargetValue(mixParam->load());

    // 8 ms downsampling window for 160-point visualizer
    scopeSamplesPerPoint = std::max(1, static_cast<int>(std::round(0.008 * safeSr)));
    scopeSampleCounter = 0;
    scopeMaxDry = 0.0f;
    scopeMaxWet = 0.0f;
    scopeMinDuck = 1.0f;

    silentThresholdSamples = static_cast<int>(safeSr); // 1.0 second
    silentSampleCounter = 0;
    audioSilentAtomic.store(false, std::memory_order_relaxed);
}

void SDVerbAudioProcessor::releaseResources()
{
    reverbEngine.reset();
    gateDucker.reset();
}

bool SDVerbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    // Output must be stereo
    if (mainOut != juce::AudioChannelSet::stereo())
        return false;

    // Input can be Stereo (default) or Mono (fallback)
    return mainIn == juce::AudioChannelSet::stereo()
        || mainIn == juce::AudioChannelSet::mono();
}

float SDVerbAudioProcessor::getRateBeats(int rateIndex) noexcept
{
    switch (rateIndex)
    {
        case 0: return 0.125f;          // 1/32
        case 1: return 0.25f;           // 1/16
        case 2: return 0.5f;            // 1/8
        case 3: return 1.0f;            // 1/4
        case 4: return 1.0f / 6.0f;     // 1/16T
        case 5: return 0.75f;           // 1/8D
        default: return 0.25f;
    }
}

float SDVerbAudioProcessor::getPreDelayRateBeats(int rateIndex) noexcept
{
    switch (rateIndex)
    {
        case 0: return 0.0625f;         // 1/64
        case 1: return 0.125f;          // 1/32
        case 2: return 1.0f / 6.0f;     // 1/16T
        case 3: return 0.25f;           // 1/16
        case 4: return 1.0f / 3.0f;     // 1/8T
        case 5: return 0.5f;            // 1/8
        default: return 0.125f;
    }
}

void SDVerbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();

    if (numOutputChannels < 2 || numInputChannels < 1 || numSamples == 0)
        return;

    // Algorithm selection (auto-flushes buffers on change)
    const int algoIdx = static_cast<int>(algoParam->load());
    reverbEngine.setAlgorithm(algoIdx == 1 ? sdverb::FdnReverb::Algorithm::Metal
                                           : sdverb::FdnReverb::Algorithm::Dense);

    // Update smoothed parameter targets
    sizeSmooth.setTargetValue(sizeParam->load());
    decaySmooth.setTargetValue(decayParam->load());
    predelaySmooth.setTargetValue(predelayParam->load());
    dampingSmooth.setTargetValue(dampingParam->load());
    inHpfSmooth.setTargetValue(inHpfParam->load());
    inLpfSmooth.setTargetValue(inLpfParam->load());
    gateThreshSmooth.setTargetValue(gateThreshParam->load());
    gateHoldSmooth.setTargetValue(gateHoldParam->load());
    gateRelSmooth.setTargetValue(gateRelParam->load());
    duckAmountSmooth.setTargetValue(duckAmountParam->load());
    widthSmooth.setTargetValue(widthParam->load());
    mixSmooth.setTargetValue(mixParam->load());

    const bool pdlySync    = pdlySyncParam->load() >= 0.5f;
    const int  pdlyRateIdx = static_cast<int>(pdlyRateParam->load());
    const bool lofiEnabled = lofiEnabledParam->load() >= 0.5f;
    const bool gateEnabled = gateEnabledParam->load() >= 0.5f;
    const bool gateSync    = gateSyncParam->load() >= 0.5f;
    const int  gateRateIdx = static_cast<int>(gateRateParam->load());

    // Retrieve DAW BPM (fallback 120.0) and transport playing state
    double bpm = 120.0;
    bool isPlaying = true;
    if (auto* ph = getPlayHead())
    {
        if (const auto pos = ph->getPosition())
        {
            if (pos->getBpm().hasValue() && *pos->getBpm() > 0.0)
                bpm = *pos->getBpm();
            isPlaying = pos->getIsPlaying();
        }
    }
    lastBpmAtomic.store(bpm, std::memory_order_relaxed);

    const float* inL = buffer.getReadPointer(0);
    const float* inR = (numInputChannels >= 2) ? buffer.getReadPointer(1) : inL;
    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getWritePointer(1);

    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    constexpr float kSilenceThreshold = 3.16227766e-5f; // -90 dB

    const int syncHoldSamples = gateSync
        ? static_cast<int>(std::round((60.0 / bpm) * static_cast<double>(getRateBeats(gateRateIdx)) * sr))
        : 0;
    const float syncPreDelayMs = pdlySync
        ? std::clamp(static_cast<float>((60.0 / bpm) * static_cast<double>(getPreDelayRateBeats(pdlyRateIdx)) * 1000.0),
                     0.0f, sdverb::FdnReverb::kMaxPreDelayMs)
        : 0.0f;

    sdverb::GateDucker::ProcessResult lastGdResult {};

    for (int i = 0; i < numSamples; ++i)
    {
        const float dryL = inL[i];
        const float dryR = inR[i];

        const float curSize       = sizeSmooth.getNextValue();
        const float curDecay      = decaySmooth.getNextValue();
        const float curPredelay   = pdlySync ? syncPreDelayMs : predelaySmooth.getNextValue();
        const float curDamping    = dampingSmooth.getNextValue();
        const float curHpf        = inHpfSmooth.getNextValue();
        const float curLpf        = inLpfSmooth.getNextValue();
        const float curGateThresh = gateThreshSmooth.getNextValue();
        const float curGateHoldMs = gateHoldSmooth.getNextValue();
        const float curGateRelMs  = gateRelSmooth.getNextValue();
        const float curDuckAmt    = duckAmountSmooth.getNextValue();
        const float curWidth      = widthSmooth.getNextValue();
        const float curMix        = mixSmooth.getNextValue();

        // Update input filter coefficients if changed
        reverbEngine.updateFilters(curHpf, curLpf);

        // Compute Hold samples
        const int holdSamples = gateSync
            ? syncHoldSamples
            : static_cast<int>(std::round(static_cast<double>(curGateHoldMs) * 0.001 * sr));

        // 1. Reverb Core
        float wetL = 0.0f;
        float wetR = 0.0f;
        reverbEngine.processSample(dryL, dryR, curPredelay, curSize, curDecay, curDamping, lofiEnabled, wetL, wetR);

        // 2. Envelope Follower + Gate & Hold + Ducking
        lastGdResult = gateDucker.processSample(dryL, dryR, gateEnabled, curGateThresh, holdSamples, curGateRelMs, curDuckAmt);

        const float vcaGain = lastGdResult.gateGain * lastGdResult.duckGain;
        wetL *= vcaGain;
        wetR *= vcaGain;

        // 3. Stereo Width (M/S)
        const float mid  = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR);
        const float w    = curWidth * 0.01f;
        const float widL = mid + w * side;
        const float widR = mid - w * side;

        // 4. Dry / Wet Mix
        const float m = curMix * 0.01f;
        outL[i] = (1.0f - m) * dryL + m * widL;
        outR[i] = (1.0f - m) * dryR + m * widR;

        // 5. Downsampling for 160-point GUI Envelope Scope (every 8 ms)
        const float dryAbs = std::max(std::abs(dryL), std::abs(dryR));
        const float wetAbs = std::max(std::abs(widL), std::abs(widR));
        scopeMaxDry  = std::max(scopeMaxDry, dryAbs);
        scopeMaxWet  = std::max(scopeMaxWet, wetAbs);
        scopeMinDuck = std::min(scopeMinDuck, lastGdResult.duckGain);

        if (++scopeSampleCounter >= scopeSamplesPerPoint)
        {
            scopeFifo.push({ scopeMaxDry, scopeMaxWet, scopeMinDuck });
            scopeSampleCounter = 0;
            scopeMaxDry = 0.0f;
            scopeMaxWet = 0.0f;
            scopeMinDuck = 1.0f;
        }

        // 6. Idle silence detection (stopped & < -90 dB for 1 second)
        if (!isPlaying && dryAbs < kSilenceThreshold && wetAbs < kSilenceThreshold)
        {
            if (silentSampleCounter < silentThresholdSamples)
                ++silentSampleCounter;
        }
        else
        {
            silentSampleCounter = 0;
        }
    }

    gateStateAtomic.store(static_cast<int>(lastGdResult.state), std::memory_order_relaxed);
    duckGainAtomic.store(lastGdResult.duckGain, std::memory_order_relaxed);
    audioSilentAtomic.store(silentSampleCounter >= silentThresholdSamples, std::memory_order_relaxed);
}

double SDVerbAudioProcessor::getTailLengthSeconds() const
{
    const float decay = (decayParam != nullptr) ? decayParam->load() : 0.80f;
    const float pre   = (predelayParam != nullptr) ? predelayParam->load() * 0.001f : 0.01f;
    return static_cast<double>(decay + pre + 0.1f);
}

int SDVerbAudioProcessor::getNumPrograms()
{
    return kNumFactoryPresets;
}

int SDVerbAudioProcessor::getCurrentProgram()
{
    return juce::jlimit(0, kNumFactoryPresets - 1, currentProgramAtomic.load(std::memory_order_relaxed));
}

void SDVerbAudioProcessor::setCurrentProgram(int index)
{
    const int idx = juce::jlimit(0, kNumFactoryPresets - 1, index);
    currentProgramAtomic.store(idx, std::memory_order_relaxed);
    const auto& p = kFactoryPresets[idx];

    auto setParamValue = [this](const char* id, float plainValue)
    {
        if (auto* param = apvts.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(plainValue));
    };

    setParamValue("algo",         static_cast<float>(p.algo));
    setParamValue("size",         p.size);
    setParamValue("decay",        p.decay);
    setParamValue("pdly_sync",    p.pdlySync ? 1.0f : 0.0f);
    setParamValue("predelay",     p.predelay);
    setParamValue("pdly_rate",    static_cast<float>(p.pdlyRate));
    setParamValue("damping",      p.damping);
    setParamValue("in_hpf",       p.inHpf);
    setParamValue("in_lpf",       p.inLpf);
    setParamValue("gate_enabled", p.gateEnabled ? 1.0f : 0.0f);
    setParamValue("gate_thresh",  p.gateThresh);
    setParamValue("gate_sync",    p.gateSync ? 1.0f : 0.0f);
    setParamValue("gate_hold",    p.gateHold);
    setParamValue("gate_rate",    static_cast<float>(p.gateRate));
    setParamValue("gate_rel",     p.gateRel);
    setParamValue("duck_amount",  p.duckAmount);
    setParamValue("lofi_enabled", p.lofiEnabled ? 1.0f : 0.0f);
    setParamValue("width",        p.width);
    setParamValue("mix",          p.mix);
}

const juce::String SDVerbAudioProcessor::getProgramName(int index)
{
    if (index >= 0 && index < kNumFactoryPresets)
        return kFactoryPresets[index].name;
    return {};
}

juce::AudioProcessorEditor* SDVerbAudioProcessor::createEditor()
{
    return new SDVerbAudioProcessorEditor(*this);
}

void SDVerbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr)
    {
        xml->setAttribute("uiTheme", savedThemeIdAtomic.load(std::memory_order_relaxed));
        xml->setAttribute("currentProgram", currentProgramAtomic.load(std::memory_order_relaxed));
        copyXmlToBinary(*xml, destData);
    }
}

void SDVerbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        const int themeId = juce::jlimit(0, 3, xmlState->getIntAttribute("uiTheme", 0));
        savedThemeIdAtomic.store(themeId, std::memory_order_relaxed);

        const int progIdx = juce::jlimit(0, kNumFactoryPresets - 1, xmlState->getIntAttribute("currentProgram", 0));
        currentProgramAtomic.store(progIdx, std::memory_order_relaxed);

        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SDVerbAudioProcessor();
}

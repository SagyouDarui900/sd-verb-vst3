#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "dsp/FdnReverb.h"
#include "dsp/GateDucker.h"

namespace sdverb
{

struct ScopePoint
{
    float dryPeak = 0.0f;
    float wetPeak = 0.0f;
    float duckGain = 1.0f;
};

/**
 * Lock-free SPSC FIFO for transferring 8 ms downsampled envelope points to the GUI thread.
 */
class ScopeFifo
{
public:
    static constexpr int kCapacity = 256;

    void reset() noexcept
    {
        fifo.reset();
        buffer.fill({});
    }

    void push(const ScopePoint& point) noexcept
    {
        const auto scope = fifo.write(1);
        if (scope.blockSize1 > 0)
            buffer[static_cast<size_t>(scope.startIndex1)] = point;
    }

    template <typename Callback>
    void drain(Callback&& onPoint) noexcept
    {
        const int numReady = fifo.getNumReady();
        if (numReady <= 0)
            return;

        const auto scope = fifo.read(numReady);
        for (int i = 0; i < scope.blockSize1; ++i)
            onPoint(buffer[static_cast<size_t>(scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i)
            onPoint(buffer[static_cast<size_t>(scope.startIndex2 + i)]);
    }

private:
    juce::AbstractFifo fifo { kCapacity };
    std::array<ScopePoint, kCapacity> buffer {};
};

} // namespace sdverb

class SDVerbAudioProcessor : public juce::AudioProcessor
{
public:
    SDVerbAudioProcessor();
    ~SDVerbAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    sdverb::ScopeFifo& getScopeFifo() noexcept { return scopeFifo; }

    int getGateStateAtomic() const noexcept { return gateStateAtomic.load(std::memory_order_relaxed); }
    float getDuckGainAtomic() const noexcept { return duckGainAtomic.load(std::memory_order_relaxed); }
    bool isAudioSilent() const noexcept { return audioSilentAtomic.load(std::memory_order_relaxed); }
    double getLastKnownBpm() const noexcept { return lastBpmAtomic.load(std::memory_order_relaxed); }

    int getSavedThemeId() const noexcept { return savedThemeIdAtomic.load(std::memory_order_relaxed); }
    void setSavedThemeId(int themeId) noexcept { savedThemeIdAtomic.store(themeId, std::memory_order_relaxed); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    static float getRateBeats(int rateIndex) noexcept;

    juce::AudioProcessorValueTreeState apvts;

    // Raw parameter pointers (atomic)
    std::atomic<float>* algoParam        = nullptr;
    std::atomic<float>* sizeParam        = nullptr;
    std::atomic<float>* decayParam       = nullptr;
    std::atomic<float>* predelayParam    = nullptr;
    std::atomic<float>* dampingParam     = nullptr;
    std::atomic<float>* inHpfParam       = nullptr;
    std::atomic<float>* inLpfParam       = nullptr;
    std::atomic<float>* gateEnabledParam = nullptr;
    std::atomic<float>* gateThreshParam  = nullptr;
    std::atomic<float>* gateSyncParam    = nullptr;
    std::atomic<float>* gateHoldParam    = nullptr;
    std::atomic<float>* gateRateParam    = nullptr;
    std::atomic<float>* gateRelParam     = nullptr;
    std::atomic<float>* duckAmountParam  = nullptr;
    std::atomic<float>* widthParam       = nullptr;
    std::atomic<float>* mixParam         = nullptr;

    // 5.0 ms SmoothedValues for continuous parameters
    juce::SmoothedValue<float> sizeSmooth;
    juce::SmoothedValue<float> decaySmooth;
    juce::SmoothedValue<float> predelaySmooth;
    juce::SmoothedValue<float> dampingSmooth;
    juce::SmoothedValue<float> inHpfSmooth;
    juce::SmoothedValue<float> inLpfSmooth;
    juce::SmoothedValue<float> gateThreshSmooth;
    juce::SmoothedValue<float> gateHoldSmooth;
    juce::SmoothedValue<float> gateRelSmooth;
    juce::SmoothedValue<float> duckAmountSmooth;
    juce::SmoothedValue<float> widthSmooth;
    juce::SmoothedValue<float> mixSmooth;

    // DSP engines
    sdverb::FdnReverb reverbEngine;
    sdverb::GateDucker gateDucker;

    // Downsampling & GUI state communication
    sdverb::ScopeFifo scopeFifo;
    int scopeSamplesPerPoint = 352;
    int scopeSampleCounter = 0;
    float scopeMaxDry = 0.0f;
    float scopeMaxWet = 0.0f;
    float scopeMinDuck = 1.0f;

    int silentSampleCounter = 0;
    int silentThresholdSamples = 44100;

    std::atomic<int> gateStateAtomic { static_cast<int>(sdverb::GateDucker::GateState::Bypass) };
    std::atomic<float> duckGainAtomic { 1.0f };
    std::atomic<bool> audioSilentAtomic { false };
    std::atomic<double> lastBpmAtomic { 120.0 };
    std::atomic<int> savedThemeIdAtomic { 0 };
    std::atomic<int> currentProgramAtomic { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SDVerbAudioProcessor)
};

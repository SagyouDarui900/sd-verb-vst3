#pragma once

#include <algorithm>
#include <cmath>

namespace sdverb
{

/**
 * Shared Peak Envelope Follower, 3-State Gate & Hold, and Asymmetric IIR Ducking Processor.
 */
class GateDucker
{
public:
    enum class GateState : int
    {
        Bypass = 0,
        Open   = 1,
        Hold   = 2,
        Shut   = 3
    };

    void prepare(double newSampleRate) noexcept
    {
        sampleRate = (newSampleRate > 0.0) ? newSampleRate : 44100.0;

        // Envelope Follower: Attack 1.0 ms, Release 50.0 ms
        envAttackAlpha  = std::exp(-1.0f / (0.001f * static_cast<float>(sampleRate)));
        envReleaseAlpha = std::exp(-1.0f / (0.050f * static_cast<float>(sampleRate)));

        // Ducking Smoother: Attack 2.0 ms, Release 80.0 ms
        duckAttackAlpha  = std::exp(-1.0f / (0.002f * static_cast<float>(sampleRate)));
        duckReleaseAlpha = std::exp(-1.0f / (0.080f * static_cast<float>(sampleRate)));

        // Gate open smoothing (1.5 ms) to avoid click on re-open
        gateOpenAlpha = std::exp(-1.0f / (0.0015f * static_cast<float>(sampleRate)));

        lastGateThreshDb = -999.0f;
        lastGateReleaseMs = -1.0f;

        reset();
    }

    void reset() noexcept
    {
        envIn = 0.0f;
        gateGain = 1.0f;
        duckGain = 1.0f;
        holdCounter = 0;
        currentGateState = GateState::Shut;
    }

    struct ProcessResult
    {
        float envLinear = 0.0f;
        float gateGain  = 1.0f;
        float duckGain  = 1.0f;
        GateState state = GateState::Bypass;
    };

    /**
     * Process 1 stereo sample of sidechain detection and compute Gate & Ducking gains.
     */
    inline ProcessResult processSample(float dryL, float dryR,
                                       bool gateEnabled, float gateThreshDb,
                                       int holdSamples, float gateReleaseMs,
                                       float duckAmountPercent) noexcept
    {
        // 1. Shared Peak Envelope Follower
        const float xPeak = std::max(std::abs(dryL), std::abs(dryR));
        if (xPeak > envIn)
            envIn = envAttackAlpha * envIn + (1.0f - envAttackAlpha) * xPeak;
        else
            envIn = envReleaseAlpha * envIn + (1.0f - envReleaseAlpha) * xPeak;

        // 2. 3-State Gate & Hold (with -3.0 dB hysteresis)
        if (!gateEnabled)
        {
            currentGateState = GateState::Bypass;
            gateGain = 1.0f;
            holdCounter = 0;
        }
        else
        {
            if (std::abs(gateThreshDb - lastGateThreshDb) > 1.0e-4f)
            {
                lastGateThreshDb = gateThreshDb;
                cachedOpenThreshLin  = std::pow(10.0f, gateThreshDb / 20.0f);
                cachedCloseThreshLin = std::pow(10.0f, (gateThreshDb - 3.0f) / 20.0f);
            }

            const bool shouldStayOpen = (currentGateState == GateState::Open)
                                            ? (envIn >= cachedCloseThreshLin)
                                            : (envIn >= cachedOpenThreshLin);

            if (shouldStayOpen)
            {
                currentGateState = GateState::Open;
                holdCounter = 0;
                gateGain = gateOpenAlpha * gateGain + (1.0f - gateOpenAlpha) * 1.0f;
            }
            else if (holdCounter < holdSamples)
            {
                currentGateState = GateState::Hold;
                ++holdCounter;
                gateGain = gateOpenAlpha * gateGain + (1.0f - gateOpenAlpha) * 1.0f;
            }
            else
            {
                currentGateState = GateState::Shut;
                if (std::abs(gateReleaseMs - lastGateReleaseMs) > 1.0e-4f)
                {
                    lastGateReleaseMs = gateReleaseMs;
                    const float relSec = std::max(0.001f, gateReleaseMs * 0.001f);
                    cachedRelAlpha = std::exp(-1.0f / (relSec * static_cast<float>(sampleRate)));
                }
                gateGain = cachedRelAlpha * gateGain;
            }
        }

        // 3. Built-in Reverb Ducking (Fixed threshold -36.0 dB)
        const float envDb = 20.0f * std::log10(std::max(envIn, 1.0e-5f));
        const float over = std::clamp((envDb - (-36.0f)) / 36.0f, 0.0f, 1.0f);
        const float duckAmount = std::clamp(duckAmountPercent * 0.01f, 0.0f, 1.0f);
        const float gTarget = 1.0f - duckAmount * over;

        if (gTarget < duckGain)
            duckGain = duckAttackAlpha * duckGain + (1.0f - duckAttackAlpha) * gTarget;
        else
            duckGain = duckReleaseAlpha * duckGain + (1.0f - duckReleaseAlpha) * gTarget;

        return { envIn, gateGain, duckGain, currentGateState };
    }

private:
    double sampleRate = 44100.0;

    float envAttackAlpha  = 0.99f;
    float envReleaseAlpha = 0.999f;
    float duckAttackAlpha  = 0.99f;
    float duckReleaseAlpha = 0.999f;
    float gateOpenAlpha = 0.95f;

    float envIn = 0.0f;
    float gateGain = 1.0f;
    float duckGain = 1.0f;
    int holdCounter = 0;
    GateState currentGateState = GateState::Shut;

    float lastGateThreshDb = -999.0f;
    float cachedOpenThreshLin = 0.0630957f;
    float cachedCloseThreshLin = 0.0446684f;
    float lastGateReleaseMs = -1.0f;
    float cachedRelAlpha = 0.999f;
};

} // namespace sdverb

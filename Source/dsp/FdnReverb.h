#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace sdverb
{

/**
 * 2nd-order Butterworth Biquad filter (allocation-free for audio thread).
 */
struct BiquadFilter
{
    void reset() noexcept
    {
        z1 = 0.0f;
        z2 = 0.0f;
    }

    void setLowPass(double sampleRate, float cutoffHz) noexcept
    {
        const double clampedFreq = std::clamp(static_cast<double>(cutoffHz), 10.0, sampleRate * 0.49);
        const double w0 = 2.0 * 3.14159265358979323846 * clampedFreq / sampleRate;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        constexpr double invSqrt2 = 0.70710678118654752440; // Butterworth Q = 1/sqrt(2)
        const double alpha = sinW0 * 0.5 / invSqrt2;
        const double a0Inv = 1.0 / (1.0 + alpha);

        b0 = static_cast<float>(((1.0 - cosW0) * 0.5) * a0Inv);
        b1 = static_cast<float>((1.0 - cosW0) * a0Inv);
        b2 = b0;
        a1 = static_cast<float>((-2.0 * cosW0) * a0Inv);
        a2 = static_cast<float>((1.0 - alpha) * a0Inv);
    }

    void setHighPass(double sampleRate, float cutoffHz) noexcept
    {
        const double clampedFreq = std::clamp(static_cast<double>(cutoffHz), 10.0, sampleRate * 0.49);
        const double w0 = 2.0 * 3.14159265358979323846 * clampedFreq / sampleRate;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        constexpr double invSqrt2 = 0.70710678118654752440;
        const double alpha = sinW0 * 0.5 / invSqrt2;
        const double a0Inv = 1.0 / (1.0 + alpha);

        b0 = static_cast<float>(((1.0 + cosW0) * 0.5) * a0Inv);
        b1 = static_cast<float>(-(1.0 + cosW0) * a0Inv);
        b2 = b0;
        a1 = static_cast<float>((-2.0 * cosW0) * a0Inv);
        a2 = static_cast<float>((1.0 - alpha) * a0Inv);
    }

    inline float process(float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;
};

/**
 * Simple 1-pole IIR low-pass damping filter.
 */
struct OnePoleLPF
{
    void reset() noexcept { state = 0.0f; }

    inline float process(float x, float alpha) noexcept
    {
        state = (1.0f - alpha) * x + alpha * state;
        return state;
    }

private:
    float state = 0.0f;
};

/**
 * Pre-allocated circular delay line with linear interpolation.
 */
class DelayLine
{
public:
    void allocate(int maxSamples)
    {
        // Power-of-two or exact size + guard samples
        buffer.assign(static_cast<size_t>(maxSamples + 4), 0.0f);
        writeIndex = 0;
    }

    void reset() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    inline void push(float sample) noexcept
    {
        buffer[static_cast<size_t>(writeIndex)] = sample;
        if (++writeIndex >= static_cast<int>(buffer.size()))
            writeIndex = 0;
    }

    inline float readLinear(float delaySamples) const noexcept
    {
        const int size = static_cast<int>(buffer.size());
        const float clampedDelay = std::clamp(delaySamples, 1.0f, static_cast<float>(size - 2));
        const int dInt = static_cast<int>(clampedDelay);
        const float frac = clampedDelay - static_cast<float>(dInt);

        int idx0 = writeIndex - dInt;
        if (idx0 < 0) idx0 += size;
        int idx1 = idx0 - 1;
        if (idx1 < 0) idx1 += size;

        const float y0 = buffer[static_cast<size_t>(idx0)];
        const float y1 = buffer[static_cast<size_t>(idx1)];
        return y0 + frac * (y1 - y0);
    }

    inline float readInt(int delaySamples) const noexcept
    {
        const int size = static_cast<int>(buffer.size());
        const int clampedDelay = std::clamp(delaySamples, 1, size - 2);
        int idx = writeIndex - clampedDelay;
        if (idx < 0) idx += size;
        return buffer[static_cast<size_t>(idx)];
    }

private:
    std::vector<float> buffer;
    int writeIndex = 0;
};

/**
 * SD-Verb Dual-Algorithm Reverb Engine:
 *   Mode 0 (DENSE): 8ch Hadamard FDN (Ambience-inspired)
 *   Mode 1 (METAL): 4-parallel Comb + 2-series Allpass per channel (SH-1/Solidverb-inspired)
 * Includes Pre-Delay and 2nd-order Butterworth Input HPF/LPF.
 */
class FdnReverb
{
public:
    enum class Algorithm
    {
        Dense = 0,
        Metal = 1
    };

    static constexpr double kMaxSupportedSampleRate = 192000.0;
    static constexpr double kBaseSampleRate = 44100.0;
    static constexpr float kMaxPreDelayMs = 100.0f;

    // 44.1 kHz base prime delay lengths for DENSE (8ch FDN)
    static constexpr std::array<int, 8> kDenseBaseDelays = {
        1117, 1277, 1451, 1627, 1811, 2017, 2237, 2473
    };

    // 44.1 kHz base delay lengths for METAL (4 Comb + 2 Allpass)
    static constexpr std::array<int, 4> kMetalCombBaseDelays = {
        1117, 1277, 1451, 1627
    };
    static constexpr std::array<int, 2> kMetalAllpassBaseDelays = {
        225, 556
    };
    static constexpr int kMetalRightOffsetSamples = 23;
    static constexpr float kAllpassGain = 0.5f;

    void prepare(double newSampleRate)
    {
        sampleRate = (newSampleRate > 0.0) ? newSampleRate : 44100.0;

        // Pre-allocate buffers to accommodate up to 192 kHz & Size = 100% & Pre-Delay = 100 ms
        const double maxRatio = std::max(sampleRate, kMaxSupportedSampleRate) / kBaseSampleRate;
        const int maxPreDelaySamples = static_cast<int>(std::ceil(kMaxPreDelayMs * 0.001 * std::max(sampleRate, kMaxSupportedSampleRate))) + 16;

        preDelayL.allocate(maxPreDelaySamples);
        preDelayR.allocate(maxPreDelaySamples);

        for (int i = 0; i < 8; ++i)
        {
            const int maxLen = static_cast<int>(std::ceil(kDenseBaseDelays[static_cast<size_t>(i)] * maxRatio)) + 16;
            denseDelays[static_cast<size_t>(i)].allocate(maxLen);
        }

        for (int i = 0; i < 4; ++i)
        {
            const int maxLen = static_cast<int>(std::ceil((kMetalCombBaseDelays[static_cast<size_t>(i)] + kMetalRightOffsetSamples) * maxRatio)) + 16;
            metalCombL[static_cast<size_t>(i)].allocate(maxLen);
            metalCombR[static_cast<size_t>(i)].allocate(maxLen);
        }

        for (int i = 0; i < 2; ++i)
        {
            const int maxLen = static_cast<int>(std::ceil((kMetalAllpassBaseDelays[static_cast<size_t>(i)] + kMetalRightOffsetSamples) * maxRatio)) + 16;
            metalAllpassL[static_cast<size_t>(i)].allocate(maxLen);
            metalAllpassR[static_cast<size_t>(i)].allocate(maxLen);
        }

        lastHpfHz = -1.0f;
        lastLpfHz = -1.0f;
        lastDampingPercent = -1.0f;
        lastDenseSizeFactor = -1.0f;
        lastDenseDecaySec = -1.0f;
        lastMetalSizeFactor = -1.0f;
        lastMetalDecaySec = -1.0f;

        reset();
        updateFilters(120.0f, 12000.0f);
    }

    void reset() noexcept
    {
        preDelayL.reset();
        preDelayR.reset();
        hpfL.reset();
        hpfR.reset();
        lpfL.reset();
        lpfR.reset();
        resetReverbState();
    }

    void resetReverbState() noexcept
    {
        for (auto& d : denseDelays) d.reset();
        for (auto& f : denseDamp) f.reset();

        for (auto& d : metalCombL) d.reset();
        for (auto& d : metalCombR) d.reset();
        for (auto& f : metalDampL) f.reset();
        for (auto& f : metalDampR) f.reset();
        for (auto& ap : metalAllpassL) ap.reset();
        for (auto& ap : metalAllpassR) ap.reset();
    }

    void setAlgorithm(Algorithm newAlgo) noexcept
    {
        if (currentAlgo != newAlgo)
        {
            currentAlgo = newAlgo;
            resetReverbState();
        }
    }

    void updateFilters(float hpfHz, float lpfHz) noexcept
    {
        if (std::abs(hpfHz - lastHpfHz) > 0.01f)
        {
            lastHpfHz = hpfHz;
            hpfL.setHighPass(sampleRate, hpfHz);
            hpfR.setHighPass(sampleRate, hpfHz);
        }
        if (std::abs(lpfHz - lastLpfHz) > 0.01f)
        {
            lastLpfHz = lpfHz;
            lpfL.setLowPass(sampleRate, lpfHz);
            lpfR.setLowPass(sampleRate, lpfHz);
        }
    }

    inline float getDampingAlpha(float dampingPercent) noexcept
    {
        if (std::abs(dampingPercent - lastDampingPercent) > 1.0e-4f)
        {
            lastDampingPercent = dampingPercent;
            const float d = std::clamp(dampingPercent * 0.01f, 0.0f, 1.0f);
            const float fDamp = 20000.0f * std::pow(0.05f, d);
            cachedDampAlpha = std::exp(-2.0f * 3.14159265358979323846f * fDamp / static_cast<float>(sampleRate));
        }
        return cachedDampAlpha;
    }

    /**
     * Process a single stereo sample through Pre-Delay -> Input HPF/LPF -> Reverb Core.
     * Outputs pure Wet stereo signal in (outL, outR).
     */
    inline void processSample(float inL, float inR,
                              float preDelayMs, float sizePercent,
                              float decaySec, float dampingPercent,
                              float& outL, float& outR) noexcept
    {
        // 1. Pre-Delay (read-before-write with continuous linear interpolation down to 0 ms)
        const float preDelaySamples = std::max(0.0f, preDelayMs * 0.001f * static_cast<float>(sampleRate));
        float pdL = inL;
        float pdR = inR;
        if (preDelaySamples >= 1.0f)
        {
            pdL = preDelayL.readLinear(preDelaySamples);
            pdR = preDelayR.readLinear(preDelaySamples);
        }
        else if (preDelaySamples > 0.0f)
        {
            const float prev1L = preDelayL.readInt(1);
            const float prev1R = preDelayR.readInt(1);
            pdL = inL + preDelaySamples * (prev1L - inL);
            pdR = inR + preDelaySamples * (prev1R - inR);
        }
        preDelayL.push(inL);
        preDelayR.push(inR);

        // 2. Input HPF -> LPF
        const float filtL = lpfL.process(hpfL.process(pdL));
        const float filtR = lpfR.process(hpfR.process(pdR));

        // 3. Common damping coefficient & size/decay parameters
        const float dampAlpha = getDampingAlpha(dampingPercent);
        const float sizeFactor = std::clamp(sizePercent * 0.01f, 0.10f, 1.00f);
        const float safeDecay = std::max(0.05f, decaySec);
        const float srRatio = static_cast<float>(sampleRate / kBaseSampleRate);

        if (currentAlgo == Algorithm::Dense)
        {
            processDense(filtL, filtR, sizeFactor, safeDecay, dampAlpha, srRatio, outL, outR);
        }
        else
        {
            processMetal(filtL, filtR, sizeFactor, safeDecay, dampAlpha, srRatio, outL, outR);
        }
    }

private:
    inline float computeFeedbackGain(float delaySamples, float decaySec) const noexcept
    {
        const float tau = delaySamples / static_cast<float>(sampleRate);
        return std::pow(10.0f, -3.0f * tau / decaySec);
    }

    inline void processDense(float inL, float inR,
                             float sizeFactor, float decaySec, float dampAlpha, float srRatio,
                             float& outL, float& outR) noexcept
    {
        if (std::abs(sizeFactor - lastDenseSizeFactor) > 1.0e-5f
            || std::abs(decaySec - lastDenseDecaySec) > 1.0e-5f)
        {
            lastDenseSizeFactor = sizeFactor;
            lastDenseDecaySec = decaySec;
            for (int i = 0; i < 8; ++i)
            {
                const float len = std::max(1.0f, static_cast<float>(kDenseBaseDelays[static_cast<size_t>(i)]) * srRatio * sizeFactor);
                cachedDenseLen[static_cast<size_t>(i)] = len;
                cachedDenseG[static_cast<size_t>(i)] = computeFeedbackGain(len, decaySec);
            }
        }

        std::array<float, 8> dOut {};
        for (int i = 0; i < 8; ++i)
        {
            const float raw = denseDelays[static_cast<size_t>(i)].readLinear(cachedDenseLen[static_cast<size_t>(i)]);
            dOut[static_cast<size_t>(i)] = denseDamp[static_cast<size_t>(i)].process(raw, dampAlpha);
        }

        // 8-point Fast Walsh-Hadamard Transform (FWHT)
        std::array<float, 8> h = dOut;
        // Stage 1 (stride 1)
        for (int i = 0; i < 8; i += 2)
        {
            const float a = h[static_cast<size_t>(i)];
            const float b = h[static_cast<size_t>(i + 1)];
            h[static_cast<size_t>(i)] = a + b;
            h[static_cast<size_t>(i + 1)] = a - b;
        }
        // Stage 2 (stride 2)
        for (int i = 0; i < 8; i += 4)
        {
            for (int j = 0; j < 2; ++j)
            {
                const float a = h[static_cast<size_t>(i + j)];
                const float b = h[static_cast<size_t>(i + j + 2)];
                h[static_cast<size_t>(i + j)] = a + b;
                h[static_cast<size_t>(i + j + 2)] = a - b;
            }
        }
        // Stage 3 (stride 4)
        for (int j = 0; j < 4; ++j)
        {
            const float a = h[static_cast<size_t>(j)];
            const float b = h[static_cast<size_t>(j + 4)];
            h[static_cast<size_t>(j)] = a + b;
            h[static_cast<size_t>(j + 4)] = a - b;
        }

        constexpr float invSqrt8 = 0.35355339059327376220f; // 1 / sqrt(8)
        for (int i = 0; i < 8; ++i)
        {
            const float g = cachedDenseG[static_cast<size_t>(i)];
            const float inj = (i % 2 == 0) ? inL : inR;
            const float dIn = inj + g * (invSqrt8 * h[static_cast<size_t>(i)]);
            denseDelays[static_cast<size_t>(i)].push(dIn);
        }

        // L = 0.5 * sum(even), R = 0.5 * sum(odd)
        outL = 0.5f * (dOut[0] + dOut[2] + dOut[4] + dOut[6]);
        outR = 0.5f * (dOut[1] + dOut[3] + dOut[5] + dOut[7]);
    }

    inline void processMetal(float inL, float inR,
                             float sizeFactor, float decaySec, float dampAlpha, float srRatio,
                             float& outL, float& outR) noexcept
    {
        if (std::abs(sizeFactor - lastMetalSizeFactor) > 1.0e-5f
            || std::abs(decaySec - lastMetalDecaySec) > 1.0e-5f)
        {
            lastMetalSizeFactor = sizeFactor;
            lastMetalDecaySec = decaySec;
            for (int i = 0; i < 4; ++i)
            {
                const float baseL = static_cast<float>(kMetalCombBaseDelays[static_cast<size_t>(i)]);
                const float baseR = baseL + static_cast<float>(kMetalRightOffsetSamples);
                const float lenL = std::max(1.0f, baseL * srRatio * sizeFactor);
                const float lenR = std::max(1.0f, baseR * srRatio * sizeFactor);
                cachedMetalCombLenL[static_cast<size_t>(i)] = lenL;
                cachedMetalCombLenR[static_cast<size_t>(i)] = lenR;
                cachedMetalCombGL[static_cast<size_t>(i)] = computeFeedbackGain(lenL, decaySec);
                cachedMetalCombGR[static_cast<size_t>(i)] = computeFeedbackGain(lenR, decaySec);
            }
            for (int i = 0; i < 2; ++i)
            {
                const float baseL = static_cast<float>(kMetalAllpassBaseDelays[static_cast<size_t>(i)]);
                const float baseR = baseL + static_cast<float>(kMetalRightOffsetSamples);
                cachedMetalApLenL[static_cast<size_t>(i)] = std::max(1.0f, baseL * srRatio * sizeFactor);
                cachedMetalApLenR[static_cast<size_t>(i)] = std::max(1.0f, baseR * srRatio * sizeFactor);
            }
        }

        float sumL = 0.0f;
        float sumR = 0.0f;

        // 4 parallel feedback comb filters per channel (linear interpolation to avoid zipper noise)
        for (int i = 0; i < 4; ++i)
        {
            const float lenL = cachedMetalCombLenL[static_cast<size_t>(i)];
            const float lenR = cachedMetalCombLenR[static_cast<size_t>(i)];

            const float cOutL = metalCombL[static_cast<size_t>(i)].readLinear(lenL);
            const float cOutR = metalCombR[static_cast<size_t>(i)].readLinear(lenR);

            const float dampL = metalDampL[static_cast<size_t>(i)].process(cOutL, dampAlpha);
            const float dampR = metalDampR[static_cast<size_t>(i)].process(cOutR, dampAlpha);

            metalCombL[static_cast<size_t>(i)].push(inL + cachedMetalCombGL[static_cast<size_t>(i)] * dampL);
            metalCombR[static_cast<size_t>(i)].push(inR + cachedMetalCombGR[static_cast<size_t>(i)] * dampR);

            sumL += cOutL;
            sumR += cOutR;
        }

        // Gain-matched normalization for 4 parallel combs (closer to DENSE RMS)
        float sigL = sumL * 0.38f;
        float sigR = sumR * 0.38f;

        // 2 series Schroeder allpass filters per channel
        for (int i = 0; i < 2; ++i)
        {
            const float lenL = cachedMetalApLenL[static_cast<size_t>(i)];
            const float lenR = cachedMetalApLenR[static_cast<size_t>(i)];

            const float bufOutL = metalAllpassL[static_cast<size_t>(i)].readLinear(lenL);
            const float vL = sigL + kAllpassGain * bufOutL;
            metalAllpassL[static_cast<size_t>(i)].push(vL);
            sigL = bufOutL - kAllpassGain * vL;

            const float bufOutR = metalAllpassR[static_cast<size_t>(i)].readLinear(lenR);
            const float vR = sigR + kAllpassGain * bufOutR;
            metalAllpassR[static_cast<size_t>(i)].push(vR);
            sigR = bufOutR - kAllpassGain * vR;
        }

        outL = sigL;
        outR = sigR;
    }

    double sampleRate = 44100.0;
    Algorithm currentAlgo = Algorithm::Dense;

    DelayLine preDelayL;
    DelayLine preDelayR;
    BiquadFilter hpfL, hpfR;
    BiquadFilter lpfL, lpfR;
    float lastHpfHz = -1.0f;
    float lastLpfHz = -1.0f;

    float lastDampingPercent = -1.0f;
    float cachedDampAlpha = 0.5f;

    // DENSE state & coefficient cache
    std::array<DelayLine, 8> denseDelays;
    std::array<OnePoleLPF, 8> denseDamp;
    float lastDenseSizeFactor = -1.0f;
    float lastDenseDecaySec = -1.0f;
    std::array<float, 8> cachedDenseLen {};
    std::array<float, 8> cachedDenseG {};

    // METAL state & coefficient cache
    std::array<DelayLine, 4> metalCombL;
    std::array<DelayLine, 4> metalCombR;
    std::array<OnePoleLPF, 4> metalDampL;
    std::array<OnePoleLPF, 4> metalDampR;
    std::array<DelayLine, 2> metalAllpassL;
    std::array<DelayLine, 2> metalAllpassR;
    float lastMetalSizeFactor = -1.0f;
    float lastMetalDecaySec = -1.0f;
    std::array<float, 4> cachedMetalCombLenL {};
    std::array<float, 4> cachedMetalCombLenR {};
    std::array<float, 4> cachedMetalCombGL {};
    std::array<float, 4> cachedMetalCombGR {};
    std::array<float, 2> cachedMetalApLenL {};
    std::array<float, 2> cachedMetalApLenR {};
};

} // namespace sdverb

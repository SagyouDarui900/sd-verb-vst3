#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include "../PluginProcessor.h"
#include "RetroLookAndFeel.h"

namespace sdverb
{

/**
 * Top-Right Visualizer (Y2K high-contrast opaque vector style):
 *  - Left:  160-point Realtime Envelope Scope + capsule header badge + spec string
 *  - Right: Cached Theoretical Reverb Shape Guide + capsule header badge + 45-deg parallel lines + 4 vertical dot rings
 */
class VerbVisualizer : public juce::Component
{
public:
    static constexpr int kScopePoints = 160;

    VerbVisualizer(SDVerbAudioProcessor& proc, RetroLookAndFeel& laf)
        : processor(proc), lookAndFeel(laf)
    {
        history.fill({ 0.0f, 0.0f, 1.0f });
        setOpaque(false);
    }

    void pushNewAudioData()
    {
        bool receivedAny = false;
        processor.getScopeFifo().drain([this, &receivedAny](const ScopePoint& pt)
        {
            history[static_cast<size_t>(writeIdx)] = pt;
            writeIdx = (writeIdx + 1) % kScopePoints;
            receivedAny = true;
        });

        if (receivedAny)
            repaint();
    }

    void updateShapeParameters(float preDelayMs, float decaySec, bool gateActive,
                               float gateThreshDb, float holdMs, float gateRelMs, float widthPercent)
    {
        cachedPreDelayMs   = preDelayMs;
        cachedDecaySec     = decaySec;
        cachedGateActive   = gateActive;
        cachedGateThreshDb = gateThreshDb;
        cachedHoldMs       = holdMs;
        cachedGateRelMs    = gateRelMs;
        cachedWidthPercent = widthPercent;

        rebuildShapePath();
        repaint();
    }

    void resized() override
    {
        rebuildShapePath();
    }

    void paint(juce::Graphics& g) override
    {
        const auto& pal = lookAndFeel.getCurrentPalette();
        auto bounds = getLocalBounds().toFloat();

        const float guideWidth = juce::jmin(bounds.getWidth() * 0.30f, bounds.getHeight() * 0.92f);
        const auto scopeBounds = bounds.withTrimmedRight(guideWidth + 2.0f).reduced(6.0f, 6.0f);
        const auto guideBounds = bounds.removeFromRight(guideWidth).reduced(6.0f, 6.0f);

        // Crisp 1.5px solid divider between Scope and Shape Guide
        g.setColour(pal.textMain);
        g.fillRect(guideBounds.getX() - 6.5f, 0.0f, 1.5f, bounds.getHeight());

        paintScope(g, scopeBounds, pal);
        paintShapeGuide(g, guideBounds, pal);
    }

private:
    void paintScope(juce::Graphics& g, const juce::Rectangle<float>& area, const ColorPalette& pal)
    {
        if (area.getWidth() <= 4.0f || area.getHeight() <= 4.0f)
            return;

        // Top capsule header badge: ENVELOPE SCOPE (1.28s)
        // (Right side of top bar is reserved for the preset selector ComboBox in PluginEditor)
        const float badgeH = 17.0f;
        const float badgeW = juce::jmin(182.0f, area.getWidth());
        const auto badgeRect = juce::Rectangle<float>(area.getX(), area.getY(), badgeW, badgeH);
        g.setColour(pal.mainAccent);
        g.fillRoundedRectangle(badgeRect, badgeH * 0.5f);

        g.setColour(pal.textInverted);
        g.setFont(lookAndFeel.getHeavyFontOptions(9.8f));
        g.drawFittedText("ENVELOPE SCOPE (1.28s)", badgeRect.reduced(6.0f, 0.0f).toNearestInt(),
                         juce::Justification::centred, 1, 0.85f);

        // Reserve left gutter for dB scale labels and top padding below header badge
        const float labelW = 34.0f;
        const auto plotArea = area.withTrimmedLeft(labelW).withTrimmedTop(24.0f);
        if (plotArea.getWidth() <= 4.0f || plotArea.getHeight() <= 4.0f)
            return;

        // Solid opaque grid lines (0 dB, -24 dB, -48 dB)
        g.setColour(pal.subDecor);
        const float y0db  = plotArea.getY();
        const float y24db = plotArea.getY() + plotArea.getHeight() * 0.4f;
        const float y48db = plotArea.getY() + plotArea.getHeight() * 0.8f;
        g.drawHorizontalLine(static_cast<int>(y0db),  plotArea.getX(), plotArea.getRight());
        g.drawHorizontalLine(static_cast<int>(y24db), plotArea.getX(), plotArea.getRight());
        g.drawHorizontalLine(static_cast<int>(y48db), plotArea.getX(), plotArea.getRight());

        // dB scale labels on the left
        g.setColour(pal.textMain);
        g.setFont(lookAndFeel.getNumericFontOptions(10.5f));
        g.drawText("0dB",   juce::Rectangle<float>(area.getX(), y0db - 6.0f,  labelW - 4.0f, 12.0f), juce::Justification::centredRight, false);
        g.drawText("-24dB", juce::Rectangle<float>(area.getX(), y24db - 6.0f, labelW - 4.0f, 12.0f), juce::Justification::centredRight, false);
        g.drawText("-48dB", juce::Rectangle<float>(area.getX(), y48db - 6.0f, labelW - 4.0f, 12.0f), juce::Justification::centredRight, false);

        auto ampToY = [&plotArea](float amp) -> float
        {
            const float db = 20.0f * std::log10(std::max(amp, 1.0e-3f)); // -60 dB .. 0 dB
            const float norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
            return plotArea.getBottom() - norm * plotArea.getHeight();
        };

        juce::Path dryPath;
        juce::Path wetFillPath;
        juce::Path wetStrokePath;

        const float stepX = plotArea.getWidth() / static_cast<float>(kScopePoints - 1);

        dryPath.startNewSubPath(plotArea.getX(), plotArea.getBottom());
        wetFillPath.startNewSubPath(plotArea.getX(), plotArea.getBottom());

        for (int i = 0; i < kScopePoints; ++i)
        {
            const int idx = (writeIdx + i) % kScopePoints;
            const auto& pt = history[static_cast<size_t>(idx)];
            const float x = plotArea.getX() + static_cast<float>(i) * stepX;
            const float yDry = ampToY(pt.dryPeak);
            const float yWet = ampToY(pt.wetPeak);

            dryPath.lineTo(x, yDry);
            wetFillPath.lineTo(x, yWet);

            if (i == 0)
                wetStrokePath.startNewSubPath(x, yWet);
            else
                wetStrokePath.lineTo(x, yWet);
        }

        dryPath.lineTo(plotArea.getRight(), plotArea.getBottom());
        dryPath.closeSubPath();

        wetFillPath.lineTo(plotArea.getRight(), plotArea.getBottom());
        wetFillPath.closeSubPath();

        // Layer 1: Dry input (opaque subDecor)
        g.setColour(pal.subDecor);
        g.fillPath(dryPath);

        // Layer 2: Wet reverb (opaque mainAccent + crisp 1.5px dark/inverted contour)
        g.setColour(pal.mainAccent);
        g.fillPath(wetFillPath);
        g.setColour(pal.textMain);
        g.strokePath(wetStrokePath, juce::PathStrokeType(1.5f));

        // Baseline crisp border
        g.setColour(pal.textMain);
        g.drawLine(plotArea.getX(), plotArea.getBottom(), plotArea.getRight(), plotArea.getBottom(), 1.2f);

        // Top Ducking Gain Bar (opaque high-contrast drop-down bar)
        const float duckGain = juce::jlimit(0.0f, 1.0f, processor.getDuckGainAtomic());
        const float duckReduction = 1.0f - duckGain;
        if (duckReduction > 0.005f)
        {
            const float barHeight = juce::jmax(3.0f, duckReduction * (plotArea.getHeight() * 0.25f));
            g.setColour(pal.headerBandBg);
            g.fillRect(plotArea.getX(), plotArea.getY(), plotArea.getWidth(), barHeight);
        }

        // Layer 3: Gate Threshold dashed line (when Gate Active = On)
        if (cachedGateActive)
        {
            const float normThresh = juce::jlimit(0.0f, 1.0f, (cachedGateThreshDb + 60.0f) / 60.0f);
            const float threshY = plotArea.getBottom() - normThresh * plotArea.getHeight();
            const float dashes[2] = { 4.0f, 3.0f };
            g.setColour(pal.textMain);
            g.drawDashedLine(juce::Line<float>(plotArea.getX(), threshY, plotArea.getRight(), threshY), dashes, 2, 1.2f);
        }
    }

    void paintShapeGuide(juce::Graphics& g, const juce::Rectangle<float>& area, const ColorPalette& pal)
    {
        if (area.getWidth() <= 4.0f || area.getHeight() <= 4.0f)
            return;

        // Top capsule header badge: SHAPE GUIDE
        const float badgeH = 17.0f;
        const float badgeW = juce::jmin(112.0f, area.getWidth() - 16.0f);
        const auto badgeRect = juce::Rectangle<float>(area.getX(), area.getY(), badgeW, badgeH);
        g.setColour(pal.headerBandBg);
        g.fillRoundedRectangle(badgeRect, badgeH * 0.5f);

        g.setColour(pal.textInverted);
        g.setFont(lookAndFeel.getHeavyFontOptions(9.8f));
        g.drawFittedText("SHAPE GUIDE", badgeRect.reduced(6.0f, 0.0f).toNearestInt(),
                         juce::Justification::centred, 1, 0.85f);

        // Y2K vector decor on right edge: 2 parallel 45-degree bent lines + 4 vertical dot rings
        {
            const float rx = area.getRight() - 3.0f;
            const float topY = area.getY() + 3.0f;
            const float bendY1 = topY + 26.0f;
            const float bendY2 = bendY1 + 12.0f;
            const float botY = area.getBottom() - 28.0f;

            juce::Path pLines;
            for (int k = 0; k < 2; ++k)
            {
                const float off = static_cast<float>(k) * 4.0f;
                pLines.startNewSubPath(area.getRight() - 26.0f, topY + off);
                pLines.lineTo(rx - 12.0f - off, topY + off);
                pLines.lineTo(rx - off, topY + 12.0f + off * 2.0f);
                pLines.lineTo(rx - off, bendY1);
                pLines.lineTo(rx - 7.0f - off, bendY2);
                pLines.lineTo(rx - 7.0f - off, botY);
            }
            g.setColour(pal.mainAccent);
            g.strokePath(pLines, juce::PathStrokeType(1.2f));

            // 4 vertical circular dot rings (o o o o)
            const float dotCx = area.getRight() - 6.0f;
            const float dotStartY = area.getY() + 62.0f;
            const float dotDia = 5.5f;
            const float dotStep = 10.0f;
            for (int i = 0; i < 4; ++i)
            {
                const float cy = dotStartY + static_cast<float>(i) * dotStep;
                if (cy + dotDia < area.getBottom() - 24.0f)
                {
                    if (i == 0)
                        g.fillEllipse(dotCx - dotDia * 0.5f, cy - dotDia * 0.5f, dotDia, dotDia);
                    else
                        g.drawEllipse(dotCx - dotDia * 0.5f, cy - dotDia * 0.5f, dotDia, dotDia, 1.3f);
                }
            }
        }

        // Cached opaque envelope shape
        g.setColour(pal.subDecor);
        g.fillPath(cachedShapeFill);
        g.setColour(pal.mainAccent);
        g.strokePath(cachedShapeStroke, juce::PathStrokeType(2.0f));

        // Stereo Width Bar at bottom: L --- C --- R
        const auto widthBarArea = area.toNearestInt().removeFromBottom(20).toFloat();
        const float midX = widthBarArea.getCentreX();
        const float midY = widthBarArea.getCentreY();
        const float maxHalfW = (widthBarArea.getWidth() - 28.0f) * 0.5f;
        const float curHalfW = maxHalfW * juce::jlimit(0.0f, 1.0f, cachedWidthPercent / 200.0f);

        g.setColour(pal.subDecor);
        g.drawLine(midX - maxHalfW, midY, midX + maxHalfW, midY, 2.0f);

        g.setColour(pal.mainAccent);
        g.drawLine(midX - curHalfW, midY, midX + curHalfW, midY, 3.2f);
        g.setColour(pal.textMain);
        g.fillEllipse(midX - 2.8f, midY - 2.8f, 5.6f, 5.6f);

        g.setColour(pal.textMain);
        g.setFont(lookAndFeel.getNumericFontOptions(11.0f));
        g.drawText("L", juce::Rectangle<float>(widthBarArea.getX(), midY - 7.0f, 14.0f, 14.0f), juce::Justification::centredLeft, false);
        g.drawText("R", juce::Rectangle<float>(widthBarArea.getRight() - 14.0f, midY - 7.0f, 14.0f, 14.0f), juce::Justification::centredRight, false);
    }

    void rebuildShapePath()
    {
        cachedShapeStroke.clear();
        cachedShapeFill.clear();

        const auto bounds = getLocalBounds().toFloat();
        const float guideWidth = juce::jmin(bounds.getWidth() * 0.30f, bounds.getHeight() * 0.92f);
        const auto area = bounds.withWidth(guideWidth).withX(bounds.getRight() - guideWidth)
                                .reduced(8.0f, 24.0f)
                                .withTrimmedTop(2.0f)
                                .withTrimmedBottom(6.0f)
                                .withTrimmedRight(16.0f);

        if (area.getWidth() <= 4.0f || area.getHeight() <= 4.0f)
            return;

        // Auto-scale horizontal axis to fit total duration cleanly
        const float decayMs = cachedDecaySec * 1000.0f;
        float totalMs = 100.0f;
        if (cachedGateActive)
            totalMs = juce::jmax(80.0f, (cachedPreDelayMs + cachedHoldMs + cachedGateRelMs) * 1.12f);
        else
            totalMs = juce::jmax(100.0f, (cachedPreDelayMs + decayMs) * 1.12f);

        const float w = area.getWidth();
        const float preFrac = juce::jlimit(0.0f, 0.55f, cachedPreDelayMs / totalMs);
        const float startX = area.getX() + preFrac * w;
        const float topY = area.getY();
        const float botY = area.getBottom();

        cachedShapeStroke.startNewSubPath(area.getX(), botY);
        cachedShapeStroke.lineTo(startX, botY);
        cachedShapeStroke.lineTo(startX, topY);

        if (cachedGateActive)
        {
            const float holdFrac = juce::jlimit(0.02f, 0.85f, cachedHoldMs / totalMs);
            const float holdEndX = juce::jmin(area.getRight() - 4.0f, startX + holdFrac * w);
            const float decayRatio = juce::jlimit(0.04f, 0.65f, cachedHoldMs / juce::jmax(50.0f, decayMs));
            const float holdEndY = topY + (botY - topY) * decayRatio;

            const float relFrac = juce::jlimit(0.02f, 0.50f, cachedGateRelMs / totalMs);
            const float relEndX = juce::jmin(area.getRight(), holdEndX + relFrac * w);

            cachedShapeStroke.lineTo(holdEndX, holdEndY);
            cachedShapeStroke.lineTo(relEndX, botY);
            if (relEndX < area.getRight())
                cachedShapeStroke.lineTo(area.getRight(), botY);
        }
        else
        {
            const float decayFrac = juce::jlimit(0.08f, 0.95f, decayMs / totalMs);
            const float endX = juce::jmin(area.getRight(), startX + decayFrac * w);
            cachedShapeStroke.quadraticTo(startX + (endX - startX) * 0.30f, botY - (botY - topY) * 0.35f, endX, botY);
            if (endX < area.getRight())
                cachedShapeStroke.lineTo(area.getRight(), botY);
        }

        cachedShapeFill = cachedShapeStroke;
        cachedShapeFill.closeSubPath();
    }

    SDVerbAudioProcessor& processor;
    RetroLookAndFeel& lookAndFeel;

    std::array<ScopePoint, kScopePoints> history {};
    int writeIdx = 0;

    float cachedPreDelayMs   = 10.0f;
    float cachedDecaySec     = 0.80f;
    bool  cachedGateActive   = false;
    float cachedGateThreshDb = -24.0f;
    float cachedHoldMs       = 80.0f;
    float cachedGateRelMs    = 30.0f;
    float cachedWidthPercent = 100.0f;

    juce::Path cachedShapeStroke;
    juce::Path cachedShapeFill;
};

} // namespace sdverb

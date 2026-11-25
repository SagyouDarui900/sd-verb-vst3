#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>
#include <array>
#include <cmath>

namespace sdverb
{

enum class ThemeId : int
{
    LightOrange = 0,
    DarkOrange  = 1,
    LightCyan   = 2,
    DarkCyan    = 3
};

struct ColorPalette
{
    juce::Colour baseBg;
    juce::Colour sidebarBg;
    juce::Colour mainAccent;
    juce::Colour subDecor;
    juce::Colour textMain;
    juce::Colour textInverted;
    juce::Colour headerBandBg;
};

inline ColorPalette getPalette(ThemeId id) noexcept
{
    switch (id)
    {
        case ThemeId::LightOrange:
            return {
                juce::Colour(0xFFFFFFFF), // baseBg (pure white)
                juce::Colour(0xFFFF9800), // sidebarBg
                juce::Colour(0xFFFF8C00), // mainAccent
                juce::Colour(0xFFFFCC80), // subDecor (opaque light orange)
                juce::Colour(0xFF3B2E25), // textMain (dark brown)
                juce::Colour(0xFFFFFFFF), // textInverted (pure white)
                juce::Colour(0xFF3B2E25)  // headerBandBg (dark brown)
            };
        case ThemeId::DarkOrange:
            return {
                juce::Colour(0xFF1E1F22), // baseBg (dark charcoal)
                juce::Colour(0xFFE67E00), // sidebarBg
                juce::Colour(0xFFFF9800), // mainAccent
                juce::Colour(0xFF663D00), // subDecor (opaque dark amber)
                juce::Colour(0xFFF2F3F5), // textMain (off-white)
                juce::Colour(0xFFFFFFFF), // textInverted
                juce::Colour(0xFFCC6E00)  // headerBandBg (solid dark orange for white text)
            };
        case ThemeId::LightCyan:
            return {
                juce::Colour(0xFFFFFFFF),
                juce::Colour(0xFF00A8E8),
                juce::Colour(0xFF0096D6),
                juce::Colour(0xFF80D8FF),
                juce::Colour(0xFF1E2A32),
                juce::Colour(0xFFFFFFFF),
                juce::Colour(0xFF1E2A32)
            };
        case ThemeId::DarkCyan:
            return {
                juce::Colour(0xFF1B2024),
                juce::Colour(0xFF0088BD),
                juce::Colour(0xFF00B4F0),
                juce::Colour(0xFF004C6B),
                juce::Colour(0xFFEDF2F7),
                juce::Colour(0xFFFFFFFF),
                juce::Colour(0xFF0077A8)
            };
        default:
            return getPalette(ThemeId::LightOrange);
    }
}

/**
 * Y2K / BMS BGA-inspired high-contrast opaque vector LookAndFeel:
 *  - No translucent fills or light-gray strokes
 *  - Butt-capped value arc + 9 outer dot ticks + double-ring solid center circle
 *  - Solid accent/base fills with crisp 1.5px solid borders
 */
class RetroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RetroLookAndFeel()
    {
        heavyTypeface = juce::Typeface::createSystemTypefaceFor(
            BinaryData::EncodeSansExpandedBlack_ttf,
            BinaryData::EncodeSansExpandedBlack_ttfSize);
        numericTypeface = juce::Typeface::createSystemTypefaceFor(
            BinaryData::ChakraPetchSemiBold_ttf,
            BinaryData::ChakraPetchSemiBold_ttfSize);
        setTheme(ThemeId::LightOrange);
    }

    juce::FontOptions getHeavyFontOptions(float height) const
    {
        if (heavyTypeface != nullptr)
            return juce::FontOptions(heavyTypeface).withHeight(height);
        return juce::FontOptions(height, juce::Font::bold);
    }

    juce::FontOptions getNumericFontOptions(float height) const
    {
        if (numericTypeface != nullptr)
            return juce::FontOptions(numericTypeface).withHeight(height);
        return juce::FontOptions(height, juce::Font::bold);
    }

    juce::Font getLabelFont(juce::Label& label) override
    {
        if (dynamic_cast<juce::Slider*>(label.getParentComponent()) != nullptr)
        {
            const float h = juce::jmax(12.5f, static_cast<float>(label.getHeight()) * 0.85f);
            return juce::Font(getNumericFontOptions(h));
        }
        return juce::LookAndFeel_V4::getLabelFont(label);
    }

    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        const float h = juce::jlimit(10.0f, 13.0f, static_cast<float>(box.getHeight()) * 0.64f);
        return juce::Font(getNumericFontOptions(h));
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font(getNumericFontOptions(12.5f));
    }

    void setTheme(ThemeId newTheme) noexcept
    {
        theme = newTheme;
        palette = getPalette(newTheme);

        setColour(juce::Label::textColourId, palette.textMain);
        setColour(juce::ComboBox::backgroundColourId, palette.baseBg);
        setColour(juce::ComboBox::textColourId, palette.textMain);
        setColour(juce::ComboBox::outlineColourId, palette.mainAccent);
        setColour(juce::PopupMenu::backgroundColourId, palette.baseBg);
        setColour(juce::PopupMenu::textColourId, palette.textMain);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, palette.mainAccent);
        setColour(juce::PopupMenu::highlightedTextColourId, palette.textInverted);
    }

    const ColorPalette& getCurrentPalette() const noexcept { return palette; }
    ThemeId getThemeId() const noexcept { return theme; }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, const float rotaryStartAngle,
                          const float rotaryEndAngle, juce::Slider& slider) override
    {
        const float boundsSize = static_cast<float>(juce::jmin(width, height));
        const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        const float dotOrbitR  = boundsSize * 0.455f;
        const float dotSize    = juce::jmax(2.6f, boundsSize * 0.055f);
        const float arcRadius  = boundsSize * 0.365f;
        const float thickness  = juce::jmax(2.2f, boundsSize * 0.068f);

        // 1. 9 evenly-spaced circular scale dots around the outer perimeter
        for (int i = 0; i < 9; ++i)
        {
            const float t = static_cast<float>(i) / 8.0f;
            const float dotAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
            const float dx = cx + std::sin(dotAngle) * dotOrbitR;
            const float dy = cy - std::cos(dotAngle) * dotOrbitR;

            const bool reached = (sliderPos + 0.001f) >= t;
            g.setColour((slider.isEnabled() && reached) ? palette.mainAccent : palette.textMain);
            g.fillEllipse(dx - dotSize * 0.5f, dy - dotSize * 0.5f, dotSize, dotSize);
        }

        // 2. Track & Value arc with flat butt-capped endpoints
        {
            juce::Path bgArc;
            bgArc.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
            g.setColour(palette.subDecor);
            g.strokePath(bgArc, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }

        if (sliderPos > 0.001f)
        {
            juce::Path valArc;
            valArc.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
            g.setColour(slider.isEnabled() ? palette.mainAccent : palette.textMain);
            g.strokePath(valArc, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }

        // 3. Opaque center double-ring circle (baseBg fill + 1.5px outer outline + inner accent ring)
        const float outerCapR = arcRadius * 0.72f;
        const float innerCapR = outerCapR * 0.76f;

        g.setColour(palette.baseBg);
        g.fillEllipse(cx - outerCapR, cy - outerCapR, outerCapR * 2.0f, outerCapR * 2.0f);

        g.setColour(palette.textMain);
        g.drawEllipse(cx - outerCapR, cy - outerCapR, outerCapR * 2.0f, outerCapR * 2.0f, 1.5f);

        g.setColour(slider.isEnabled() ? palette.mainAccent : palette.textMain);
        g.drawEllipse(cx - innerCapR, cy - innerCapR, innerCapR * 2.0f, innerCapR * 2.0f, 1.2f);

        // 4. Center dot + straight pointer line to the inner ring
        const float centerDotR = juce::jmax(1.8f, outerCapR * 0.16f);
        g.setColour(slider.isEnabled() ? palette.textMain : palette.subDecor);
        g.fillEllipse(cx - centerDotR, cy - centerDotR, centerDotR * 2.0f, centerDotR * 2.0f);

        const float px = cx + std::sin(angle) * innerCapR;
        const float py = cy - std::cos(angle) * innerCapR;
        g.drawLine(cx, cy, px, py, juce::jmax(1.6f, boundsSize * 0.036f));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&, bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        const float corner = bounds.getHeight() * 0.5f;
        const bool isOn = button.getToggleState();

        if (isOn)
        {
            g.setColour(shouldDrawButtonAsDown ? palette.mainAccent.darker(0.15f) : palette.mainAccent);
            g.fillRoundedRectangle(bounds, corner);
            g.setColour(palette.mainAccent);
            g.drawRoundedRectangle(bounds, corner, 1.5f);
        }
        else
        {
            g.setColour(shouldDrawButtonAsHighlighted ? palette.subDecor : palette.baseBg);
            g.fillRoundedRectangle(bounds, corner);
            g.setColour(palette.mainAccent);
            g.drawRoundedRectangle(bounds, corner, 1.5f);
        }
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool, bool) override
    {
        const bool isOn = button.getToggleState();
        g.setColour(isOn ? palette.textInverted : palette.textMain);
        const float fontSize = juce::jmin(12.0f, static_cast<float>(button.getHeight()) * 0.52f);
        g.setFont(getHeavyFontOptions(fontSize));
        g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(3, 0), juce::Justification::centred, 1, 0.80f);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);
        const float corner = bounds.getHeight() * 0.5f;
        g.setColour(palette.baseBg);
        g.fillRoundedRectangle(bounds, corner);
        g.setColour(palette.mainAccent);
        g.drawRoundedRectangle(bounds, corner, 1.5f);

        // Crisp Y2K downward triangle arrow on the right
        const float arrowX = bounds.getRight() - 12.0f;
        const float arrowY = bounds.getCentreY();
        juce::Path tri;
        tri.addTriangle(arrowX - 3.5f, arrowY - 2.0f,
                        arrowX + 3.5f, arrowY - 2.0f,
                        arrowX,        arrowY + 2.5f);
        g.setColour(box.isEnabled() ? palette.mainAccent : palette.subDecor);
        g.fillPath(tri);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(6, 1, box.getWidth() - 22, box.getHeight() - 2);
        label.setFont(getComboBoxFont(box));
    }

private:
    juce::Typeface::Ptr heavyTypeface;
    juce::Typeface::Ptr numericTypeface;
    ThemeId theme = ThemeId::LightOrange;
    ColorPalette palette {};
};

} // namespace sdverb

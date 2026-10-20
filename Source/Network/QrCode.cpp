#include "QrCode.h"
#include <qrcodegen.hpp>

namespace voclyrics
{
void QrCode::draw(juce::Graphics& graphics, juce::Rectangle<float> bounds, const juce::String& text)
{
    try
    {
        const auto utf8 = text.toStdString();
        const auto code = qrcodegen::QrCode::encodeText(utf8.c_str(), qrcodegen::QrCode::Ecc::LOW);
        const auto size = code.getSize();
        const auto modules = static_cast<float>(size + 8);
        const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight());
        const auto cell = side / modules;
        const auto originX = bounds.getCentreX() - side * 0.5f + 4.0f * cell;
        const auto originY = bounds.getCentreY() - side * 0.5f + 4.0f * cell;
        graphics.setColour(juce::Colours::white);
        graphics.fillRect(bounds.withSizeKeepingCentre(side, side));
        graphics.setColour(juce::Colour(0xff11151c));
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                if (code.getModule(x, y))
                    graphics.fillRect(originX + static_cast<float>(x) * cell,
                                      originY + static_cast<float>(y) * cell,
                                      cell + 0.25f, cell + 0.25f);
    }
    catch (...)
    {
        graphics.setColour(juce::Colours::white);
        graphics.fillRect(bounds);
        graphics.setColour(juce::Colours::black);
        graphics.drawFittedText("QR", bounds.toNearestInt(), juce::Justification::centred, 1);
    }
}
}

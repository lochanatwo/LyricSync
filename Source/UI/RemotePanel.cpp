#include "RemotePanel.h"
#include "Network/NetworkUtils.h"
#include "Network/QrCode.h"
#include <algorithm>
#include <iterator>
#include <limits>

namespace
{
constexpr const char* bandRoles[] = { "Main Singer", "Backup Singer", "Main Guitar", "Rythm Guitar",
                                      "Drummer", "Percussionist", "Bass", "Keys" };
}

RemotePanel::RemotePanel(VocLyricsAudioProcessor& p) : processor(p), deviceList(*this)
{
    addAndMakeVisible(enabled);
    addAndMakeVisible(address);
    addAndMakeVisible(portLabel);
    addAndMakeVisible(interfaceChoice);
    addAndMakeVisible(controllerLabel);
    addAndMakeVisible(controllerChoice);
    addAndMakeVisible(controllerChoice2);
    addAndMakeVisible(roleLabel);
    addAndMakeVisible(roleChoice);
    addAndMakeVisible(port);
    addAndMakeVisible(applyPort);
    viewport.setViewedComponent(&deviceList, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
    enabled.setToggleState(true, juce::dontSendNotification);
    enabled.onClick = [this]
    {
        if (enabled.getToggleState()) processor.getServer().start(port.getText().getIntValue());
        else processor.getServer().stop();
        repaint();
    };
    address.setJustificationType(juce::Justification::centred);
    address.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    const auto addresses = voclyrics::NetworkUtils::localIPv4Addresses();
    for (std::size_t i = 0; i < addresses.size(); ++i)
        interfaceChoice.addItem(addresses[i], static_cast<int>(i + 1));
    if (! addresses.empty()) interfaceChoice.setSelectedId(1, juce::dontSendNotification);
    interfaceChoice.onChange = [this] { refreshDevices(); };
    controllerLabel.setText("Shared scroll controllers (choose up to 2)", juce::dontSendNotification);
    controllerChoice.onChange = [this] { applyControllerChoices(); };
    controllerChoice2.onChange = [this] { applyControllerChoices(); };
    roleLabel.setText("Host band role", juce::dontSendNotification);
    for (int i = 0; i < static_cast<int>(std::size(bandRoles)); ++i)
        roleChoice.addItem(bandRoles[i], i + 1);
    roleChoice.setText(processor.getHostRole(), juce::dontSendNotification);
    roleChoice.onChange = [this] { processor.setHostRole(roleChoice.getText()); };
    portLabel.setText("Port", juce::dontSendNotification);
    port.setInputRestrictions(5, "0123456789");
    port.setText("8080", juce::dontSendNotification);
    applyPort.onClick = [this]
    {
        processor.getServer().stop();
        processor.getServer().start(port.getText().getIntValue());
        enabled.setToggleState(processor.getServer().isRunning(), juce::dontSendNotification);
        repaint();
    };
    startTimerHz(2);
    refreshDevices();
}
void RemotePanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff151a22));
    g.setColour(juce::Colour(0xff8793a3));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("REMOTE DISPLAY", 12, 10, getWidth() - 24, 22, juce::Justification::centredLeft);
    const auto selectedAddress = interfaceChoice.getText().isNotEmpty()
        ? interfaceChoice.getText() : voclyrics::NetworkUtils::preferredIPv4Address();
    const auto url = processor.getServer().isRunning()
        ? juce::String("http://") + selectedAddress + ":" + juce::String(processor.getServer().port()) + "/?v=12"
        : juce::String("Server is off");
    if (processor.getServer().isRunning())
        voclyrics::QrCode::draw(g, juce::Rectangle<float>(getWidth() * 0.5f - 65.0f, 62.0f, 130.0f, 130.0f), url);
    g.setColour(juce::Colour(0xffe6edf5));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawFittedText(url, 8, 194, getWidth() - 16, 25, juce::Justification::centred, 1);
    g.setColour(juce::Colour(0xff8793a3));
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(juce::String("BAND SCREENS  ") + juce::String(static_cast<int>(currentDevices.size())) + " / 8", 12, 380, getWidth() - 24, 22, juce::Justification::centredLeft);
}
void RemotePanel::resized()
{
    enabled.setBounds(10, 38, getWidth() - 20, 28);
    address.setBounds(8, 220, getWidth() - 16, 23);
    interfaceChoice.setBounds(8, 246, getWidth() - 16, 26);
    roleLabel.setBounds(8, 276, getWidth() - 16, 18);
    roleChoice.setBounds(8, 294, getWidth() - 16, 28);
    controllerLabel.setBounds(8, 330, getWidth() - 16, 18);
    const auto controllerWidth = (getWidth() - 22) / 2;
    controllerChoice.setBounds(8, 348, controllerWidth, 28);
    controllerChoice2.setBounds(14 + controllerWidth, 348, controllerWidth, 28);
    viewport.setBounds(8, 404, getWidth() - 16, juce::jmax(60, getHeight() - 468));
    portLabel.setBounds(12, getHeight() - 55, 44, 26);
    port.setBounds(58, getHeight() - 55, 70, 26);
    applyPort.setBounds(134, getHeight() - 55, getWidth() - 146, 26);
}
void RemotePanel::timerCallback() { refreshDevices(); }
void RemotePanel::refreshDevices()
{
    const auto devices = processor.getServer().devices();
    auto devicesChanged = devices.size() != currentDevices.size();
    if (! devicesChanged)
        for (std::size_t i = 0; i < devices.size(); ++i)
            if (devices[i].id != currentDevices[i].id || devices[i].name != currentDevices[i].name
                || devices[i].ipAddress != currentDevices[i].ipAddress)
            { devicesChanged = true; break; }
    currentDevices = devices;
    auto primaryId = processor.getServer().scrollController();
    auto secondaryId = processor.getServer().secondaryScrollController();
    const auto connected = [this](int id)
    {
        return id <= 0 || std::any_of(currentDevices.begin(), currentDevices.end(),
            [id](const auto& device) { return device.id == id; });
    };
    if (! connected(primaryId))
    {
        primaryId = secondaryId >= 0 && connected(secondaryId) ? secondaryId : 0;
        secondaryId = -1;
        processor.setSharedScrollControllers(primaryId, secondaryId);
        devicesChanged = true;
    }
    else if (secondaryId >= 0 && ! connected(secondaryId))
    {
        secondaryId = -1;
        processor.setSharedScrollControllers(primaryId, secondaryId);
        devicesChanged = true;
    }
    const auto wantedPrimary = primaryId == 0 ? 1 : primaryId + 1;
    const auto wantedSecondary = secondaryId < 0 ? 1 : (secondaryId == 0 ? 2 : secondaryId + 2);
    if (devicesChanged || controllerChoice.getSelectedId() != wantedPrimary
        || controllerChoice2.getSelectedId() != wantedSecondary)
    {
        controllerChoice.clear(juce::dontSendNotification);
        controllerChoice.addItem("Desktop host", 1);
        controllerChoice2.clear(juce::dontSendNotification);
        controllerChoice2.addItem("No second controller", 1);
        controllerChoice2.addItem("Desktop host", 2);
        for (const auto& device : currentDevices)
        {
            controllerChoice.addItem(juce::String(device.name.c_str()) + " (" + juce::String(device.ipAddress.c_str()) + ")",
                                     device.id + 1);
            controllerChoice2.addItem(juce::String(device.name.c_str()) + " (" + juce::String(device.ipAddress.c_str()) + ")",
                                      device.id + 2);
        }
        controllerChoice.setSelectedId(wantedPrimary, juce::dontSendNotification);
        controllerChoice2.setSelectedId(wantedSecondary, juce::dontSendNotification);
    }
    const auto ip = interfaceChoice.getText().isNotEmpty() ? interfaceChoice.getText() : voclyrics::NetworkUtils::preferredIPv4Address();
    const auto serverText = processor.getServer().isRunning()
        ? juce::String("Server on · ") + ip + ":" + juce::String(processor.getServer().port())
        : juce::String("Server stopped");
    address.setText(serverText,
                    juce::dontSendNotification);
    if (processor.getServer().port() > 0) port.setText(juce::String(processor.getServer().port()), juce::dontSendNotification);
    deviceList.setSize(viewport.getWidth(), static_cast<int>(currentDevices.size()) * 70);
    deviceList.repaint();
    repaint();
}

void RemotePanel::applyControllerChoices()
{
    const auto primarySelection = controllerChoice.getSelectedId();
    const auto secondarySelection = controllerChoice2.getSelectedId();
    const auto primaryId = primarySelection <= 1 ? 0 : primarySelection - 1;
    auto secondaryId = secondarySelection <= 1 ? -1 : (secondarySelection == 2 ? 0 : secondarySelection - 2);
    if (secondaryId == primaryId)
    {
        secondaryId = -1;
        controllerChoice2.setSelectedId(1, juce::dontSendNotification);
    }
    processor.setSharedScrollControllers(primaryId, secondaryId);
}

void RemotePanel::DeviceList::paint(juce::Graphics& g)
{
    int y = 2;
    for (const auto& device : parent.currentDevices)
    {
        const auto row = juce::Rectangle<int>(2, y, getWidth() - 4, 64);
        g.setColour(juce::Colour(0xff202834));
        g.fillRoundedRectangle(row.toFloat(), 7.0f);
        g.setColour(juce::Colour(0xffe6edf5));
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawFittedText(juce::String(device.name.c_str()) + "  " + juce::String(device.ipAddress.c_str()),
                         row.reduced(7, 3).removeFromTop(20), juce::Justification::centredLeft, 1);
        g.setColour(juce::Colour(0xff6ee7b7));
        g.setFont(juce::FontOptions(11.0f));
        const auto actions = row.withTrimmedTop(31).reduced(7, 0);
        g.drawFittedText("−50     reset     +50    Disconnect", actions, juce::Justification::centred, 1);
        y += 70;
    }
}

void RemotePanel::DeviceList::mouseDown(const juce::MouseEvent& event)
{
    const auto row = event.y / 70;
    if (row < 0 || row >= static_cast<int>(parent.currentDevices.size())) return;
    const auto& device = parent.currentDevices[static_cast<std::size_t>(row)];
    const auto x = event.x;
    if (x < getWidth() * 0.25) parent.processor.getServer().nudgeDevice(device.id, -50);
    else if (x < getWidth() * 0.46) parent.processor.getServer().nudgeDevice(device.id, std::numeric_limits<int>::min());
    else if (x < getWidth() * 0.64) parent.processor.getServer().nudgeDevice(device.id, 50);
    else parent.processor.getServer().disconnectDevice(device.id);
}

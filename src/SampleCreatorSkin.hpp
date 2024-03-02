/*
 * SampleCreator
 *
 * An experimental idea based on a preliminary convo. Probably best to come back later.
 *
 * Copyright Paul Walker 2024
 *
 * Released under the MIT License. See `LICENSE.md` for details
 */

#ifndef SRC_SAMPLECREATORSKIN_HPP
#define SRC_SAMPLECREATORSKIN_HPP

#include "rack.hpp"
#include <unordered_set>

#include "ui/TextField.hpp"

namespace baconpaul::samplecreator
{
struct SampleCreatorSkin
{
    struct Client
    {
        Client();
        ~Client();
        virtual void onSkinChanged() = 0;
    };

    enum Skin
    {
        LIGHT,
        DARK
    } skin{DARK};

    std::unordered_set<Client *> clients;
    void addClient(Client *c) { clients.insert(c); }
    void removeClient(Client *c) { clients.erase(c); }

    void step()
    {

        auto isDark = (skin == SampleCreatorSkin::DARK);
        auto shouldBeDark = rack::settings::preferDarkPanels;
        if (isDark != shouldBeDark)
        {
            skin = (shouldBeDark ? SampleCreatorSkin::DARK : SampleCreatorSkin::LIGHT);
            for (auto *c : clients)
            {
                c->onSkinChanged();
            }
        }
    }

    std::string fontPath, fontPathMedium;
    bool initialized{false};
    SampleCreatorSkin() {}

    void intialize()
    {
        if (initialized)
            return;
        initialized = true;

        fontPath = rack::asset::plugin(pluginInstance, "res/FiraCode-Regular.ttf");
        fontPathMedium = rack::asset::plugin(pluginInstance, "res/FiraCode-Medium.ttf");
    }

    template <typename T> T dl(const T &dark, const T &light)
    {
        if (skin == DARK)
            return dark;
        else
            return light;
    }

#define COL(n, d, l)                                                                               \
    NVGcolor n() { return dl(d, l); }

    COL(labeLText, nvgRGB(220, 220, 220), nvgRGB(20, 20, 20));

    COL(knobGradientTop, nvgRGB(90, 90, 95), nvgRGB(90, 90, 95));
    COL(knobGradientBottom, nvgRGB(50, 50, 50), nvgRGB(50, 50, 50));

    COL(knobStroke, nvgRGB(20, 20, 20), nvgRGB(50, 50, 60));
    COL(knobValueFill, nvgRGB(240, 240, 240), nvgRGB(20, 20, 20));
    COL(knobValueStroke, nvgRGB(20, 20, 20), nvgRGB(20, 20, 20));

    COL(moduleOutline, nvgRGB(100, 100, 100), nvgRGB(100, 100, 100));

    COL(panelGradientStart, nvgRGB(20, 20, 25), nvgRGB(215, 215, 220));
    COL(panelGradientEnd, nvgRGB(20, 29, 35), nvgRGB(200, 200, 205));

    COL(panelBottomRegion, nvgRGB(40, 40, 45), nvgRGB(160, 160, 170));
    COL(panelBottomStroke, nvgRGB(0, 0, 0), nvgRGB(0, 0, 0));

    COL(panelInputFill, nvgRGB(190, 190, 200), nvgRGB(190, 190, 200));
    COL(panelInputBorder, nvgRGB(140, 140, 150), nvgRGB(140, 140, 150));
    COL(panelInputText, nvgRGB(40, 40, 50), nvgRGB(40, 40, 50));

    COL(panelOutputFill, nvgRGB(60, 60, 70), nvgRGB(60, 60, 70));
    COL(panelOutputBorder, nvgRGB(90, 90, 100), nvgRGB(40, 40, 50));
    COL(panelOutputText, nvgRGB(190, 190, 200), nvgRGB(190, 190, 200));

    COL(panelBrandText, nvgRGB(200, 200, 220), nvgRGB(0, 0, 0));

    COL(paramDisplayBorder, nvgRGB(120, 120, 120), nvgRGB(120, 120, 120));
    COL(paramDisplayBG, nvgRGB(50, 50, 60), nvgRGB(50, 50, 60));
    COL(paramDisplayText, nvgRGB(0xFF, 0x90, 0x00), nvgRGB(0xFF, 0x90, 0x00));

    COL(pushButtonFill, nvgRGB(90, 90, 100), nvgRGB(90, 90, 100));
    COL(pushButtonText, nvgRGB(50, 50, 60), nvgRGB(50, 50, 60));
    COL(pushButtonHoverText, nvgRGB(210, 210, 220), nvgRGB(210, 210, 220));

    COL(logText, nvgRGB(20, 240, 20), nvgRGB(20, 240, 20));
    COL(vuLevel, nvgRGB(240, 120, 0), nvgRGB(240, 120, 0));
};

extern SampleCreatorSkin sampleCreatorSkin;

inline SampleCreatorSkin::Client::Client() { sampleCreatorSkin.addClient(this); }

inline SampleCreatorSkin::Client::~Client() { sampleCreatorSkin.addClient(this); }

} // namespace baconpaul::samplecreator

#endif // SAMPLECREATOR_SAMPLECREATORSKIN_HPP

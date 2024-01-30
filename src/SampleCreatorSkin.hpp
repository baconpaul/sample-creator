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

    /*

    COL(labeLText, nvgRGB(220, 220, 220), nvgRGB(20, 20, 20));
    COL(labelRule, nvgRGB(110, 110, 120), nvgRGB(150, 150, 160));

    COL(deactivatedJogStroke, nvgRGB(60, 60, 60), nvgRGB(60, 60, 60));
    COL(deactivatedJogFill, nvgRGB(40, 40, 40), nvgRGB(40, 40, 40));
    COL(jogFill, nvgRGB(190, 190, 190), nvgRGB(190, 190, 190));
    COL(jogFillHover, nvgRGB(240, 240, 100), nvgRGB(240, 240, 100));
    COL(jogStroke, nvgRGB(220, 220, 220), nvgRGB(220, 220, 220));

    COL(helpOpen, nvgRGB(220, 220, 220), nvgRGB(220, 220, 220));
    COL(helpClose, nvgRGB(120, 120, 120), nvgRGB(120, 120, 120));

    COL(selectorFill, nvgRGB(20, 20, 30), nvgRGB(20, 20, 30));
    COL(selectorOutline, nvgRGB(0, 0, 0), nvgRGB(0, 0, 0));
    COL(selectorOutlineHighlight, nvgRGB(140, 140, 160), nvgRGB(140, 140, 160));
    COL(selectorEffect, nvgRGB(240, 240, 240), nvgRGB(240, 240, 240));
    COL(selectorCategory, nvgRGB(210, 210, 210), nvgRGB(210, 210, 210));
    COL(selectorPoly, nvgRGB(140, 140, 140), nvgRGB(140, 140, 140));

    COL(helpBorder, nvgRGB(180, 180, 180), nvgRGB(180, 180, 180));
    COL(helpBG, nvgRGB(20, 20, 20), nvgRGB(20, 20, 20));
    COL(helpText, nvgRGB(220, 220, 225), nvgRGB(220, 220, 225));

    float svgAlpha() { return dl(0.73, 0.23); }

     */

    COL(labeLText, nvgRGB(220, 220, 220), nvgRGB(20, 20, 20));

    COL(knobCenter, nvgRGB(110, 110, 120), nvgRGB(185, 185, 220));
    COL(knobEdge, nvgRGB(110, 110, 130), nvgRGB(190, 190, 225));
    COL(knobStroke, nvgRGB(20, 20, 20), nvgRGB(50, 50, 60));
    COL(knobValueFill, nvgRGB(240, 240, 240), nvgRGB(20, 20, 20));
    COL(knobValueStroke, nvgRGB(20, 20, 20), nvgRGB(20, 20, 20));

    COL(moduleOutline, nvgRGB(100, 100, 100), nvgRGB(100, 100, 100));

    COL(panelGradientStart, nvgRGB(20, 20, 25), nvgRGB(225, 225, 230));
    COL(panelGradientEnd, nvgRGB(20, 29, 35), nvgRGB(235, 235, 245));

    COL(panelBottomRegion, nvgRGB(40, 40, 45), nvgRGB(160, 160, 170));
    COL(panelBottomStroke, nvgRGB(0, 0, 0), nvgRGB(0, 0, 0));

    COL(panelInputFill, nvgRGB(190, 190, 200), nvgRGB(190, 190, 200));
    COL(panelInputBorder, nvgRGB(140, 140, 150), nvgRGB(140, 140, 150));
    COL(panelInputText, nvgRGB(40, 40, 50), nvgRGB(40, 40, 50));

    COL(panelOutputFill, nvgRGB(60, 60, 70), nvgRGB(60, 60, 70));
    COL(panelOutputBorder, nvgRGB(90, 90, 100), nvgRGB(40, 40, 50));
    COL(panelOutputText, nvgRGB(190, 190, 200), nvgRGB(190, 190, 200));

    COL(panelBrandText, nvgRGB(200, 200, 220), nvgRGB(0, 0, 0));
};

extern SampleCreatorSkin sampleCreatorSkin;

inline SampleCreatorSkin::Client::Client() { sampleCreatorSkin.addClient(this); }

inline SampleCreatorSkin::Client::~Client() { sampleCreatorSkin.addClient(this); }

} // namespace baconpaul::samplecreator

#endif // SAMPLECREATOR_SAMPLECREATORSKIN_HPP

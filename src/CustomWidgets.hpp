/*
 * SampleCreator
 *
 * An experimental idea based on a preliminary convo. Probably best to come back later.
 *
 * Copyright Paul Walker 2024
 *
 * Released under the MIT License. See `LICENSE.md` for details
 */

#ifndef SRC_CUSTOMWIDGETS_HPP
#define SRC_CUSTOMWIDGETS_HPP

#include "rack.hpp"

#include "sst/rackhelpers/module_connector.h"
#include "SampleCreatorSkin.hpp"

namespace baconpaul::samplecreator
{

struct PQObserver
{
    float val{-10242132.f};
    rack::Module *module{nullptr};
    int paramId{0};
    PQObserver(rack::Module *m, int p) : module(m), paramId(p) {}

    bool isStale()
    {
        if (!module)
            return false;
        auto pq = module->getParamQuantity(paramId);
        if (!pq)
            return false;

        if (pq->getValue() != val)
        {
            val = pq->getValue();
            return true;
        }
        return false;
    }
};

struct PQStringObserver
{
    std::string val{"867-5309"};
    rack::Module *module{nullptr};
    int paramId{0};
    PQStringObserver(rack::Module *m, int p) : module(m), paramId(p) {}

    bool isStale()
    {
        if (!module)
            return false;
        auto pq = module->getParamQuantity(paramId);
        if (!pq)
            return false;

        if (pq->getDisplayValueString() != val)
        {
            val = pq->getDisplayValueString();
            return true;
        }
        return false;
    }
};

template <NVGcolor (SampleCreatorSkin::*txtcol)(), int pt, int halign = NVG_ALIGN_CENTER>
struct SCLabel : rack::Widget, SampleCreatorSkin::Client
{
    sst::rackhelpers::ui::BufferedDrawFunctionWidget *bdw{nullptr};
    std::string label;

    static SCLabel *createCentered(const rack::Vec &ctr, int w, const std::string &label)
    {
        auto r = new SCLabel();
        r->box.pos = ctr;
        r->box.pos.x -= w / 2;
        r->box.pos.y -= pt / 2;

        r->box.size = rack::Vec(w, pt);

        r->label = label;

        r->initBDW();

        return r;
    }

    void initBDW()
    {
        bdw = new sst::rackhelpers::ui::BufferedDrawFunctionWidget(
            rack::Vec(0, 0), box.size, [this](auto *a) { drawLabel(a); });
        bdw->dirty = true;
        addChild(bdw);
    }

    void drawLabel(NVGcontext *vg)
    {
        auto fid = APP->window->loadFont(sampleCreatorSkin.fontPath)->handle;

        if (false)
        {
            nvgBeginPath(vg);
            nvgStrokeColor(vg, nvgRGB(0, 255, 0));
            nvgRect(vg, 0, 0, box.size.x, box.size.y);
            nvgStroke(vg);
        }

        nvgBeginPath(vg);
        nvgFillColor(vg, (&sampleCreatorSkin->*txtcol)());
        nvgFontFaceId(vg, fid);
        nvgFontSize(vg, pt);
        if (halign == NVG_ALIGN_RIGHT)
        {
            nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
            nvgText(vg, box.size.x, box.size.y * 0.5, label.c_str(), nullptr);
        }
        else if (halign == NVG_ALIGN_LEFT)
        {
            nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_LEFT);
            nvgText(vg, 0, box.size.y * 0.5, label.c_str(), nullptr);
        }
        else
        {
            nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER);
            nvgText(vg, box.size.x * 0.5, box.size.y * 0.5, label.c_str(), nullptr);
        }
    }

    void onSkinChanged() override { bdw->dirty = true; }
};

using InPortLabel = SCLabel<&SampleCreatorSkin::panelInputText, 11>;
using OutPortLabel = SCLabel<&SampleCreatorSkin::panelOutputText, 11>;
using PanelLabel = SCLabel<&SampleCreatorSkin::labeLText, 12, NVG_ALIGN_RIGHT>;

struct SCPanelParamDisplay : rack::Widget, SampleCreatorSkin::Client
{
    sst::rackhelpers::ui::BufferedDrawFunctionWidget *bdw{nullptr};
    rack::Module *module{nullptr};
    int paramId{0};

    std::unique_ptr<PQStringObserver> obs;

    static SCPanelParamDisplay *create(const rack::Vec &ctrLeft, int width, rack::Module *m,
                                       int paramId)
    {
        auto ht = 18;
        auto r = new SCPanelParamDisplay();

        r->module = m;
        r->paramId = paramId;

        r->obs = std::make_unique<PQStringObserver>(m, paramId);

        r->box.size = rack::Vec(width, ht);
        r->box.pos = ctrLeft;
        r->box.pos.y -= ht / 2;

        r->bdw = new sst::rackhelpers::ui::BufferedDrawFunctionWidget(
            rack::Vec(0, 0), r->box.size, [r](auto *a) { r->drawParam(a); });
        r->bdw->dirty = true;
        r->addChild(r->bdw);

        return r;
    }

    void drawParam(NVGcontext *vg)
    {
        nvgBeginPath(vg);
        nvgFillColor(vg, nvgRGB(255, 0, 0));
        nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 2);
        nvgStrokeColor(vg, sampleCreatorSkin.paramDisplayBorder());
        nvgFillColor(vg, sampleCreatorSkin.paramDisplayBG());
        nvgFill(vg);
        nvgStroke(vg);

        if (!module)
        {
            return;
        }
        auto pq = module->getParamQuantity(paramId);
        if (!pq)
        {
            return;
        }

        auto v = pq->getDisplayValueString();
        auto fid = APP->window->loadFont(sampleCreatorSkin.fontPath)->handle;

        nvgBeginPath(vg);
        nvgFillColor(vg, sampleCreatorSkin.paramDisplayText());
        nvgFontFaceId(vg, fid);
        nvgFontSize(vg, 12);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        nvgText(vg, 3, box.size.y * 0.5, v.c_str(), nullptr);
    }

    void step() override
    {
        if (obs->isStale())
        {
            bdw->dirty = true;
        }
        rack::Widget::step();
    }
    void onSkinChanged() override { bdw->dirty = true; }
};

template <int px, bool bipolar = false> struct PixelKnob : rack::Knob, SampleCreatorSkin::Client
{
    sst::rackhelpers::ui::BufferedDrawFunctionWidget *bdw{nullptr};
    bool stripMenuTypein{false};
    PixelKnob()
    {
        box.size = rack::Vec(px + 3, px + 3);
        float angleSpreadDegrees = 40.0;

        minAngle = -M_PI * (180 - angleSpreadDegrees) / 180;
        maxAngle = M_PI * (180 - angleSpreadDegrees) / 180;

        bdw = new sst::rackhelpers::ui::BufferedDrawFunctionWidget(
            rack::Vec(0, 0), box.size, [this](auto vg) { drawKnob(vg); });
        addChild(bdw);
    }

    void drawKnob(NVGcontext *vg)
    {
        float radius = px * 0.48;
        nvgBeginPath(vg);
        nvgEllipse(vg, box.size.x * 0.5, box.size.y * 0.5, radius, radius);

        auto lighter = [](auto a, auto f) {
            a.r = std::clamp(a.r * f, 0., 255.);
            a.g = std::clamp(a.g * f, 0., 255.);
            a.b = std::clamp(a.b * f, 0., 255.);
            return a;
        };
        nvgFillPaint(vg, nvgLinearGradient(vg, 0, 0, 0, box.size.y,
                                           lighter(sampleCreatorSkin.knobGradientTop(), 1.3),
                                           lighter(sampleCreatorSkin.knobGradientBottom(), 1.1)));
        nvgStrokeColor(vg, sampleCreatorSkin.knobStroke());
        nvgStrokeWidth(vg, 0.5);
        nvgFill(vg);
        nvgStroke(vg);

        nvgBeginPath(vg);
        nvgEllipse(vg, box.size.x * 0.5, box.size.y * 0.5, radius - 1.5, radius - 1.5);
        nvgFillPaint(vg,
                     nvgLinearGradient(vg, 0, 0, 0, box.size.y, sampleCreatorSkin.knobGradientTop(),
                                       sampleCreatorSkin.knobGradientBottom()));
        nvgStrokeColor(vg, sampleCreatorSkin.knobStroke());
        nvgStrokeWidth(vg, 0.5);
        nvgFill(vg);

        auto pq = getParamQuantity();
        if (!pq)
            return;

        nvgBeginPath(vg);
        float angle = rack::math::rescale(pq->getValue(), pq->getMinValue(), pq->getMaxValue(),
                                          minAngle, maxAngle);
        float startAngle = minAngle;
        if (bipolar)
            startAngle = 0;

        auto valueFill = sampleCreatorSkin.knobValueFill();

        nvgBeginPath(vg);
        nvgArc(vg, box.size.x * 0.5, box.size.y * 0.5, radius, startAngle - M_PI_2, angle - M_PI_2,
               startAngle < angle ? NVG_CW : NVG_CCW);
        nvgStrokeWidth(vg, 1);
        nvgStrokeColor(vg, valueFill);
        nvgLineCap(vg, NVG_ROUND);
        nvgStroke(vg);

        auto ox = std::sin(angle) * radius + box.size.x / 2;
        auto oy = box.size.y - (std::cos(angle) * radius + box.size.y / 2);

        auto ix = std::sin(angle) * radius * 0.4 + box.size.x / 2;
        auto iy = box.size.y - (std::cos(angle) * radius * 0.4 + box.size.y / 2);

        nvgBeginPath(vg);
        nvgMoveTo(vg, ox, oy);
        nvgLineTo(vg, ix, iy);
        nvgStrokeColor(vg, valueFill);
        nvgStrokeWidth(vg, 1);
        nvgStroke(vg);

        nvgBeginPath(vg);
        nvgEllipse(vg, ox, oy, 1.5, 1.5);
        nvgFillColor(vg, valueFill);
        nvgStrokeColor(vg, sampleCreatorSkin.knobValueStroke());
        nvgStrokeWidth(vg, 0.5);
        nvgStroke(vg);
        nvgFill(vg);
    }

    float lastVal{0.f};
    void step() override
    {
        bool dirty{false};

        auto pq = getParamQuantity();
        if (pq)
        {
            if (lastVal != pq->getValue())
                dirty = true;
            lastVal = pq->getValue();
        }

        if (bdw && dirty)
            bdw->dirty = dirty;

        rack::Widget::step();
    }

    void onSkinChanged() override { bdw->dirty = true; }

    void appendContextMenu(rack::Menu *menu) override
    {
        if (stripMenuTypein && menu->children.size() >= 2)
        {
            auto tgt = std::next(menu->children.begin());
            menu->removeChild(*tgt);
            delete *tgt;
        }
    }
};

struct SampleCreatorPort
    : public sst::rackhelpers::module_connector::PortConnectionMixin<rack::app::SvgPort>,
      public SampleCreatorSkin::Client
{
    SampleCreatorPort() { setPortGraphics(); }

    void setPortGraphics()
    {
        if (sampleCreatorSkin.skin == SampleCreatorSkin::DARK)
        {
            setSvg(rack::Svg::load(rack::asset::plugin(pluginInstance, "res/port_on.svg")));
        }
        else
        {
            setSvg(rack::Svg::load(rack::asset::plugin(pluginInstance, "res/port_on_light.svg")));
        }
    }

    void onSkinChanged() override { setPortGraphics(); }
};

} // namespace baconpaul::samplecreator

#endif // SAMPLECREATOR_CUSTOMWIDGETS_H
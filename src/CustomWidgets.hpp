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

#include "SampleCreatorSkin.hpp"

namespace baconpaul::samplecreator
{

template <NVGcolor (SampleCreatorSkin::*txtcol)(), int pt> struct SCLabel : rack::Widget
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

        /*nvgBeginPath(vg);
        nvgStrokeColor(vg, nvgRGB(0,255,0));
        nvgRect(vg, 0, 0, box.size.x, box.size.y);
        nvgStroke(vg);
         */

        nvgBeginPath(vg);
        nvgFillColor(vg, (&sampleCreatorSkin->*txtcol)());
        nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER);
        nvgFontFaceId(vg, fid);
        nvgFontSize(vg, pt);
        nvgText(vg, box.size.x * 0.5, box.size.y * 0.5, label.c_str(), nullptr);
    }
};

using InPortLabel = SCLabel<&SampleCreatorSkin::panelInputText, 11>;
using OutPortLabel = SCLabel<&SampleCreatorSkin::panelOutputText, 11>;

template <int px, bool bipolar = false> struct PixelKnob : rack::Knob
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
#if 0
        float radius = px * 0.48;
        nvgBeginPath(vg);
        nvgEllipse(vg, box.size.x * 0.5, box.size.y * 0.5, radius, radius);
        nvgFillPaint(vg, nvgRadialGradient(vg, box.size.x * 0.5, box.size.y * 0.5, box.size.x * 0.1,
                                           box.size.x * 0.4, sampleCreatorSkin.knobCenter(),
                                           sampleCreatorSkin.knobEdge()));
        nvgStrokeColor(vg, sampleCreatorSkin.knobStroke());
        nvgStrokeWidth(vg, 0.5);
        nvgFill(vg);
        nvgStroke(vg);

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
#endif
    }

    SampleCreatorSkin::Skin lastSkin{SampleCreatorSkin::DARK};
    float lastVal{0.f};
    void step() override
    {
        bool dirty{false};
        if (lastSkin != sampleCreatorSkin.skin)
            dirty = true;
        lastSkin = sampleCreatorSkin.skin;

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

} // namespace baconpaul::samplecreator

#endif // SAMPLECREATOR_CUSTOMWIDGETS_H

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

#include <sst/rackhelpers/ui.h>
#include "sst/rackhelpers/module_connector.h"
#include "SampleCreatorSkin.hpp"

namespace baconpaul::samplecreator
{

inline NVGcolor lighten(NVGcolor a, float f)
{
    a.r = std::clamp(a.r * f, 0.f, 255.f);
    a.g = std::clamp(a.g * f, 0.f, 255.f);
    a.b = std::clamp(a.b * f, 0.f, 255.f);
    return a;
};

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

template <NVGcolor (SampleCreatorSkin::*txtcol)(), int pt>
struct SCLabel : rack::Widget, SampleCreatorSkin::Client
{
    sst::rackhelpers::ui::BufferedDrawFunctionWidget *bdw{nullptr};
    std::string label;
    bool isControlLabel{false};

    int halign = NVG_ALIGN_CENTER;

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

    static SCLabel *create(const rack::Rect &inRect, const std::string &label)
    {
        auto r = new SCLabel();
        r->box = inRect;

        r->label = label;

        r->initBDW();
        r->halign = NVG_ALIGN_CENTER;

        return r;
    }

    static SCLabel *createCtrlLabel(const rack::Rect &inRect, const std::string &label)
    {
        auto r = new SCLabel();
        r->box = inRect;

        r->label = label;

        r->initBDW();
        r->halign = NVG_ALIGN_LEFT;
        r->isControlLabel = true;

        return r;
    }

    static SCLabel *create(const rack::Vec &ctr, int w, const std::string &label)
    {
        auto r = new SCLabel();
        r->box.pos = ctr;
        r->box.size = rack::Vec(w, pt);

        r->label = label;

        r->initBDW();
        r->halign = NVG_ALIGN_LEFT;

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

        if (isControlLabel)
        {
            float bounds[4];
            nvgTextBounds(vg, 0, box.size.y * 0.5, label.c_str(), nullptr, bounds);
            auto b1 = bounds[2] + 3;
            nvgBeginPath(vg);
            nvgMoveTo(vg, b1, box.size.y * 0.5);
            nvgLineTo(vg, box.size.x - 5, box.size.y * 0.5);
            nvgStrokeColor(vg, (&sampleCreatorSkin->*txtcol)());
            nvgStrokeWidth(vg, 0.5);
            nvgStroke(vg);
        }
    }

    void onSkinChanged() override
    {
        if (bdw)
            bdw->dirty = true;
    }
};

using InPortLabel = SCLabel<&SampleCreatorSkin::panelInputText, 11>;
using OutPortLabel = SCLabel<&SampleCreatorSkin::panelOutputText, 11>;

struct SCPanelParamDisplay : rack::ui::TextField, SampleCreatorSkin::Client
{
    sst::rackhelpers::ui::BufferedDrawFunctionWidget *bdw{nullptr};
    rack::Module *module{nullptr};
    int paramId{0};

    std::unique_ptr<PQStringObserver> obs;

    static SCPanelParamDisplay *create(const rack::Rect &inThis, rack::Module *m, int paramId)
    {
        auto ht = 18;
        auto r = new SCPanelParamDisplay();

        r->module = m;
        r->paramId = paramId;

        r->obs = std::make_unique<PQStringObserver>(m, paramId);

        r->box = inThis;

        return r;
    }

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

        return r;
    }

    double blinkTime{0.0};
    double blinkOn = true;
    void draw(const DrawArgs &args) override
    {
        auto vg = args.vg;

        nvgBeginPath(vg);
        nvgFillColor(vg, nvgRGB(255, 0, 0));
        nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 2);
        nvgStrokeColor(vg, sampleCreatorSkin.paramDisplayBorder());
        nvgFillColor(vg, sampleCreatorSkin.paramDisplayBG());
        nvgFill(vg);
        nvgStroke(vg);
    }

    void drawLayer(const DrawArgs &args, int layer) override
    {
        if (layer != 1)
            return;
        if (!module)
        {
            return;
        }
        auto pq = module->getParamQuantity(paramId);
        if (!pq)
        {
            return;
        }

        auto vg = args.vg;
        auto v = getText();
        auto fid = APP->window->loadFont(sampleCreatorSkin.fontPath)->handle;

        nvgBeginPath(vg);
        nvgFillColor(vg, sampleCreatorSkin.paramDisplayText());
        nvgFontFaceId(vg, fid);
        nvgFontSize(vg, 12);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        nvgText(vg, 3, box.size.y * 0.5, v.c_str(), nullptr);

        if (APP->event->selectedWidget == this)
        {
            int begin = std::min(cursor, selection);
            int end = std::max(cursor, selection);
            if (begin != end)
            {
                float bounds[4];
                auto css = v.substr(0, begin);
                nvgTextBounds(vg, 3, box.size.y * 0.5, css.c_str(), nullptr, bounds);
                float selStart = bounds[2];

                css = v.substr(0, end);
                nvgTextBounds(vg, 3, box.size.y * 0.5, css.c_str(), nullptr, bounds);
                float selEnd = bounds[2];

                auto c = sampleCreatorSkin.paramDisplayText();
                c.a = 0.2;
                nvgBeginPath(vg);
                nvgFillColor(vg, c);
                nvgRect(vg, selStart, 3, (selEnd - selStart), box.size.y - 6);
                nvgFill(vg);
            }

            if (blinkOn)
            {
                float bounds[4];
                auto css = v.substr(0, cursor);
                auto w = nvgTextBounds(vg, 3, box.size.y * 0.5, css.c_str(), nullptr, bounds);
                auto cpos = bounds[2];
                nvgBeginPath(vg);
                nvgRect(vg, cpos, 3, 1, box.size.y - 6);
                nvgFillColor(vg, sampleCreatorSkin.paramDisplayText());

                nvgFill(vg);
            }
            auto tm = rack::system::getTime();
            if (tm - blinkTime > 0.75)
            {
                blinkTime = tm;
                blinkOn = !blinkOn;
            }
        }
    }

    void onAction(const ActionEvent &e) override
    {
        if (!module)
            return;
        auto pq = module->getParamQuantity(paramId);
        if (!pq)
            return;

        pq->setDisplayValueString(getText());
        e.consume(this);
        Widget::onAction(e);
    }
    void step() override
    {
        if (module)
        {
            if (obs->isStale())
            {
                setText(obs->val);
                // bdw->dirty = true;
            }
        }
        rack::ui::TextField::step();
    }
    void onSkinChanged() override
    {
        if (bdw)
            bdw->dirty = true;
    }
};

struct SCPanelPushButton : rack::Widget, SampleCreatorSkin::Client
{
    sst::rackhelpers::ui::BufferedDrawFunctionWidget *bdw{nullptr};

    std::string label;
    std::function<void()> onClick{nullptr};
    std::function<bool()> isEnabled{nullptr};
    bool hover{false};
    bool enabled{true};

    enum GLYPH
    {
        NONE,
        PLAY,
        RECORD,
        STOP
    } glyph{NONE};

    static SCPanelPushButton *create(
        const rack::Vec &pos, const rack::Vec &size, const std::string &label,
        std::function<void()> onClick, std::function<bool()> isEnabled = []() { return true; })
    {
        auto res = new SCPanelPushButton();
        res->box.pos = pos;
        res->box.size = size;
        res->label = label;
        res->onClick = onClick;
        res->isEnabled = isEnabled;

        res->bdw = new sst::rackhelpers::ui::BufferedDrawFunctionWidget(
            rack::Vec(0, 0), size, [res](auto a) { res->drawButton(a); });
        res->addChild(res->bdw);
        return res;
    }

    void drawButton(NVGcontext *vg)
    {
        nvgBeginPath(vg);
        nvgFillColor(vg, nvgRGB(255, 0, 0));
        nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 2);
        nvgStrokeColor(vg, sampleCreatorSkin.paramDisplayBorder());
        nvgFillPaint(
            vg, nvgLinearGradient(
                    vg, 0, 0, 0, box.size.y,
                    lighten(sampleCreatorSkin.pushButtonFill(), enabled ? 1.4 : 1.1),
                    lighten(sampleCreatorSkin.pushButtonFill(), hover && enabled ? 1.2 : 1.0)));
        nvgFill(vg);
        nvgStroke(vg);

        auto fid = APP->window->loadFont(sampleCreatorSkin.fontPath)->handle;

        if (glyph != NONE)
        {
            switch (glyph)
            {
            case PLAY:
            {
                auto shx = 0.32;
                auto shy = 0.31;
                nvgBeginPath(vg);
                if (enabled)
                {
                    nvgFillColor(vg, nvgRGBA(20, 240, 20, hover ? 150 : 100));
                    nvgStrokeColor(vg, nvgRGB(20, 240, 20));
                }
                else
                {
                    nvgFillColor(vg, nvgRGBA(20, 20, 20, 20));
                    nvgStrokeColor(vg, nvgRGBA(20, 20, 20, 35));
                }
                nvgMoveTo(vg, box.size.x * shx, box.size.y * shy);
                nvgLineTo(vg, box.size.x * (1 - shx), box.size.y * 0.5);
                nvgLineTo(vg, box.size.x * shx, box.size.y * (1 - shy));
                nvgClosePath(vg);

                nvgFill(vg);
                nvgStroke(vg);
            }
            break;
            case STOP:
            {
                auto shr = 0.3;
                nvgBeginPath(vg);
                if (enabled)
                {
                    nvgFillColor(vg, nvgRGBA(180, 180, 180, hover ? 150 : 100));
                    nvgStrokeColor(vg, nvgRGB(20, 20, 20));
                }
                else
                {
                    nvgFillColor(vg, nvgRGBA(20, 20, 20, 20));
                    nvgStrokeColor(vg, nvgRGBA(20, 20, 20, 35));
                }
                nvgRect(vg, box.size.x * shr, box.size.y * shr, box.size.x * (1 - 2 * shr),
                        box.size.y * (1 - 2 * shr));
                nvgFill(vg);
                nvgStroke(vg);
            }
            break;
            case RECORD:
            {
                auto shr = 0.20;
                nvgBeginPath(vg);
                if (enabled)
                {
                    nvgFillColor(vg, nvgRGBA(240, 20, 20, hover ? 150 : 100));
                    nvgStrokeColor(vg, nvgRGB(240, 20, 20));
                }
                else
                {
                    nvgFillColor(vg, nvgRGBA(20, 20, 20, 20));
                    nvgStrokeColor(vg, nvgRGBA(20, 20, 20, 35));
                }
                nvgEllipse(vg, box.size.x * 0.5, box.size.y * 0.5, box.size.x * shr,
                           box.size.y * shr);

                nvgFill(vg);
                nvgStroke(vg);
            }
            break;
            default:
                break;
            }
        }
        else
        {
            nvgBeginPath(vg);
            nvgFillColor(vg, hover ? sampleCreatorSkin.pushButtonHoverText()
                                   : sampleCreatorSkin.pushButtonText());
            nvgFontFaceId(vg, fid);
            nvgFontSize(vg, 12);
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

            nvgText(vg, 3, box.size.y * 0.5, label.c_str(), nullptr);
        }
    }

    void onButton(const ButtonEvent &e) override
    {
        if (e.action == GLFW_PRESS)
        {
            if (onClick)
            {
                onClick();
                e.consume(this);
            }
        }
    }
    void onHover(const HoverEvent &e) override { e.consume(this); }
    void onEnter(const EnterEvent &e) override
    {
        hover = true;
        bdw->dirty = true;
    }
    void onLeave(const LeaveEvent &e) override
    {
        hover = false;
        bdw->dirty = true;
    }
    void onSkinChanged() override
    {
        if (bdw)
            bdw->dirty = true;
    }

    void step() override
    {
        if (isEnabled)
        {
            auto lie = isEnabled();
            if (lie != enabled)
            {
                bdw->dirty = true;
            }
            enabled = lie;
        }
        rack::Widget::step();
    }
};

struct SCPanelDropDown : rack::ParamWidget, SampleCreatorSkin::Client
{
    sst::rackhelpers::ui::BufferedDrawFunctionWidget *bdw{nullptr};

    std::function<void()> onClick{nullptr};
    bool hover{false};

    static SCPanelDropDown *create(const rack::Vec &pos, const rack::Vec &size, rack::Module *mod,
                                   int pid)
    {
        auto res = new SCPanelDropDown();
        res->box.pos = pos;
        res->box.size = size;
        res->module = mod;
        res->paramId = pid;
        res->bdw = new sst::rackhelpers::ui::BufferedDrawFunctionWidget(
            rack::Vec(0, 0), size, [res](auto a) { res->drawDropdown(a); });
        res->addChild(res->bdw);
        return res;
    }

    int buttonW = 14;
    void drawDropdown(NVGcontext *vg)
    {
        nvgBeginPath(vg);
        nvgFillColor(vg, nvgRGB(255, 0, 0));
        nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 2);
        nvgStrokeColor(vg, sampleCreatorSkin.paramDisplayBorder());
        nvgFillColor(vg, sampleCreatorSkin.paramDisplayBG());
        nvgFill(vg);
        nvgStroke(vg);

        nvgSave(vg);
        nvgScissor(vg, box.size.x - buttonW, 0, buttonW, box.size.y);
        nvgBeginPath(vg);
        nvgFillColor(vg, nvgRGB(255, 0, 0));
        nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 2);
        nvgStrokeColor(vg, sampleCreatorSkin.paramDisplayBorder());
        nvgFillPaint(
            vg, nvgLinearGradient(vg, 0, 0, 0, box.size.y,
                                  lighten(sampleCreatorSkin.pushButtonFill(), 1.4),
                                  lighten(sampleCreatorSkin.pushButtonFill(), hover ? 1.2 : 1.0)));
        nvgFill(vg);
        nvgStroke(vg);
        nvgRestore(vg);

        auto fid = APP->window->loadFont(sampleCreatorSkin.fontPath)->handle;

        nvgBeginPath(vg);
        nvgFillColor(vg, sampleCreatorSkin.paramDisplayText());
        nvgFontFaceId(vg, fid);
        nvgFontSize(vg, 12);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        auto label = std::string("");
        if (getParamQuantity())
        {
            label = getParamQuantity()->getDisplayValueString();
        }
        nvgText(vg, 3, box.size.y * 0.5, label.c_str(), nullptr);

        auto cl = rack::Vec(0, 0);
        cl.x += box.size.x - buttonW / 2;
        cl.y += box.size.y / 2;

        nvgBeginPath(vg);
        nvgMoveTo(vg, cl.x - 3, cl.y - 2);
        nvgLineTo(vg, cl.x + 3, cl.y - 2);
        nvgLineTo(vg, cl.x, cl.y + 2);
        nvgClosePath(vg);
        if (hover)
            nvgFillColor(vg, nvgRGB(50, 50, 60));
        else
            nvgFillColor(vg, nvgRGB(20, 20, 30));
        nvgFill(vg);
    }

    std::string labelCache{};
    void step() override
    {
        if (module && getParamQuantity())
        {
            if (labelCache != getParamQuantity()->getDisplayValueString())
            {
                labelCache = getParamQuantity()->getDisplayValueString();
                bdw->dirty = true;
            }
        }
        rack::ParamWidget::step();
    }
    void onButton(const ButtonEvent &e) override
    {
        if (e.action == GLFW_PRESS && e.pos.x > box.size.x - buttonW)
        {
            createContextMenu();
            e.consume(this);
        }
        else
        {
            rack::ParamWidget::onButton(e);
        }
    }
    void onHover(const HoverEvent &e) override { e.consume(this); }
    void onEnter(const EnterEvent &e) override
    {
        hover = true;
        bdw->dirty = true;
    }
    void onLeave(const LeaveEvent &e) override
    {
        hover = false;
        bdw->dirty = true;
    }
    void onSkinChanged() override
    {
        if (bdw)
            bdw->dirty = true;
    }
};

template <int px, bool bipolar = false> struct PixelKnob : rack::Knob, SampleCreatorSkin::Client
{
    sst::rackhelpers::ui::BufferedDrawFunctionWidget *bdw{nullptr}, *bdwLayer{nullptr};
    bool stripMenuTypein{false};
    PixelKnob()
    {
        box.size = rack::Vec(px + 3, px + 3);
        float angleSpreadDegrees = 40.0;

        minAngle = -M_PI * (180 - angleSpreadDegrees) / 180;
        maxAngle = M_PI * (180 - angleSpreadDegrees) / 180;

        bdw = new sst::rackhelpers::ui::BufferedDrawFunctionWidget(
            rack::Vec(0, 0), box.size, [this](auto vg) { drawKnob(vg); });
        bdwLayer = new sst::rackhelpers::ui::BufferedDrawFunctionWidgetOnLayer(
            rack::Vec(0, 0), box.size, [this](auto vg) { drawValueLayer(vg); });

        addChild(bdw);
        addChild(bdwLayer);
    }

    void drawKnob(NVGcontext *vg)
    {
        float radius = px * 0.48;
        nvgBeginPath(vg);
        nvgEllipse(vg, box.size.x * 0.5, box.size.y * 0.5, radius, radius);

        nvgFillPaint(vg, nvgLinearGradient(vg, 0, 0, 0, box.size.y,
                                           lighten(sampleCreatorSkin.knobGradientTop(), 1.3),
                                           lighten(sampleCreatorSkin.knobGradientBottom(), 1.1)));
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

    void drawValueLayer(NVGcontext *vg)
    {
        float radius = px * 0.48;

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
        {
            bdw->dirty = dirty;
            bdwLayer->dirty = dirty;
        }

        rack::Widget::step();
    }

    void onSkinChanged() override
    {
        if (bdw)
            bdw->dirty = true;
        if (bdwLayer)
            bdwLayer->dirty = true;
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

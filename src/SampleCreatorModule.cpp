/*
 * SampleCreator
 *
 * An experimental idea based on a preliminary convo. Probably best to come back later.
 *
 * Copyright Paul Walker 2024
 *
 * Released under the MIT License. See `LICENSE.md` for details
 */

#include "SampleCreator.hpp"

#include <iostream>

#include <osdialog.h>

#include "SampleCreatorSkin.hpp"
#include "CustomWidgets.hpp"
#include "RIFFWavWriter.hpp"

#include "SampleCreatorModule.hpp"

namespace baconpaul::samplecreator
{

struct SampleCreatorLogWidget : rack::Widget, SampleCreatorSkin::Client
{
    sst::rackhelpers::ui::BufferedDrawFunctionWidget *bdw{nullptr}, *bdwLayer{nullptr};

    static constexpr int linesz{9};
    SampleCreatorModule *module{nullptr};
    static SampleCreatorLogWidget *create(const rack::Vec &pos, const rack::Vec &size,
                                          SampleCreatorModule *m)
    {
        auto res = new SampleCreatorLogWidget();
        res->box.size = size;
        res->box.pos = pos;
        res->module = m;

        res->bdw = new sst::rackhelpers::ui::BufferedDrawFunctionWidget(
            rack::Vec(0, 0), size, [res](auto a) { res->drawLog(a); });
        res->addChild(res->bdw);

        res->bdwLayer = new sst::rackhelpers::ui::BufferedDrawFunctionWidgetOnLayer(
            rack::Vec(0, 0), size, [res](auto vg) { res->drawValueLayer(vg); });
        res->addChild(res->bdwLayer);

        return res;
    }

    void drawLog(NVGcontext *vg)
    {
        nvgBeginPath(vg);
        nvgStrokeColor(vg, sampleCreatorSkin.paramDisplayBorder());
        nvgFillColor(vg, sampleCreatorSkin.paramDisplayBG());
        nvgRect(vg, 0, 0, box.size.x, box.size.y);
        nvgFill(vg);
        nvgStroke(vg);
    }

    void drawValueLayer(NVGcontext *vg)
    {
        auto fid = APP->window->loadFont(sampleCreatorSkin.fontPath)->handle;

        float x = 2, y = 2;
        for (auto m : msgDeq)
        {
            nvgBeginPath(vg);
            nvgFillColor(vg, sampleCreatorSkin.logText());
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
            nvgFontFaceId(vg, fid);
            nvgFontSize(vg, linesz - 1);
            nvgText(vg, x, y, m.c_str(), nullptr);
            y += linesz;
        }
    }

    std::deque<std::string> msgDeq;
    void step() override
    {
        if (module)
        {
            while (module->hasMessage())
            {
                msgDeq.push_back(module->popMessage());
                if (msgDeq.size() > (box.size.y - 4) / linesz)
                    msgDeq.pop_front();

                bdw->dirty = true;
                bdwLayer->dirty = true;
            }
        }

        rack::Widget::step();
    }

    void onSkinChanged() override
    {
        if (bdw)
        {
            bdw->dirty = true;
        }
        if (bdwLayer)
        {
            bdwLayer->dirty = true;
        }
    }
};

struct SampleCreatorVU : rack::Widget, SampleCreatorSkin::Client
{
    SampleCreatorModule *module{nullptr};
    static SampleCreatorVU *create(const rack::Vec &pos, const rack::Vec &size,
                                   SampleCreatorModule *m)
    {
        auto res = new SampleCreatorVU();
        res->box.size = size;
        res->box.pos = pos;
        res->module = m;

        return res;
    }

    void draw(const DrawArgs &args) override
    {
        auto vg = args.vg;
        nvgBeginPath(vg);
        nvgStrokeColor(vg, sampleCreatorSkin.paramDisplayBorder());
        nvgFillColor(vg, sampleCreatorSkin.paramDisplayBG());
        nvgRect(vg, 0, 0, box.size.x, box.size.y);
        nvgFill(vg);
        nvgStroke(vg);
    }

    void drawLayer(const DrawArgs &args, int layer) override
    {
        if (!module)
            return;
        auto vg = args.vg;

        float vl[2]{module->vuLevels[0], module->vuLevels[1]};
        for (int i = 0; i < 2; ++i)
        {
            if (vl[i] < 1e-4)
                continue;
            vl[i] = vl[i] * vl[i] * vl[i];
            vl[i] = std::clamp(vl[i], 0.f, 1.f);
            nvgBeginPath(vg);
            nvgFillColor(vg, sampleCreatorSkin.vuLevel());
            nvgRect(vg, 1, 1 + i * box.size.y / 2, (box.size.x - 2) * vl[i],
                    (box.size.y - 2) / 2 - 1);
            nvgFill(vg);
        }
    }

    void onSkinChanged() override {}
};

struct SampleCreatorModuleWidget : rack::ModuleWidget, SampleCreatorSkin::Client
{
    typedef SampleCreatorModule M;

    sst::rackhelpers::ui::BufferedDrawFunctionWidget *bg{nullptr};

    SampleCreatorModuleWidget(M *m)
    {
        sampleCreatorSkin.intialize();
        setModule(m);
        box.size = rack::Vec(SCREW_WIDTH * 22, RACK_HEIGHT);

        bg = new sst::rackhelpers::ui::BufferedDrawFunctionWidget(rack::Vec(0, 0), box.size,
                                                                  [this](auto vg) { drawBG(vg); });
        bg->box.pos = rack::Vec(0.0);
        bg->box.size = box.size;
        addChild(bg);
        {
            auto sOm = new rack::widget::SvgWidget;
            auto Omsvg = APP->window->loadSvg(
                rack::asset::plugin(pluginInstance, "res/Omri_Cohen_Logo.svg"));
            sOm->setSvg(Omsvg);
            sOm->box.pos = rack::Vec(0, 0);
            sOm->wrap();

            auto tw = new rack::widget::TransformWidget();
            tw->box.pos = rack::Vec(100, box.size.y - 20);
            tw->scale(0.025);
            tw->addChild(sOm);

            addChild(tw);
        }

        {
            auto sOm = new rack::widget::SvgWidget;
            auto Omsvg =
                APP->window->loadSvg(rack::asset::plugin(pluginInstance, "res/BaconLogo.svg"));
            sOm->setSvg(Omsvg);
            sOm->box.pos = rack::Vec(0, 0);
            sOm->wrap();

            auto tw = new rack::widget::TransformWidget();
            tw->box.pos = rack::Vec(210, box.size.y - 15);
            tw->scale(0.35);
            tw->addChild(sOm);

            addChild(tw);
        }

        int headerSize{38};

        // Input Elements

        auto priorBSx = 10 * SCREW_WIDTH;
        {
            auto q = RACK_HEIGHT - 42;
            auto c1 = priorBSx * 0.25;
            auto dc = priorBSx * 0.11;
            auto c2 = priorBSx * 0.75;
            auto inl = rack::createInputCentered<SampleCreatorPort>(rack::Vec(c1 - dc, q), module,
                                                                    M::INPUT_L);
            inl->connectAsInputFromMixmaster = true;
            inl->mixMasterStereoCompanion = M::INPUT_R;

            auto lab = InPortLabel ::createCentered(rack::Vec(c1 - dc, q + 17), 20, "L");
            addChild(lab);

            auto inr = rack::createInputCentered<SampleCreatorPort>(rack::Vec(c1 + dc, q), module,
                                                                    M::INPUT_R);
            inr->connectAsInputFromMixmaster = true;
            inr->mixMasterStereoCompanion = M::INPUT_L;

            lab = InPortLabel::createCentered(rack::Vec(c1 + dc, q + 17), 20, "R");
            addChild(lab);

            lab = InPortLabel::createCentered(rack::Vec(c1, q + 17), 10, "IN");
            addChild(lab);

            addInput(inl);
            addInput(inr);
        }

        {
            auto q = RACK_HEIGHT - 42;
            auto c1 = priorBSx * 0.14 + priorBSx * 0.5;

            for (auto [o, l] : {
                     std::make_pair(M::OUTPUT_VOCT, "V/OCT"),
                     {M::OUTPUT_VELOCITY, "VEL"},
                     {M::OUTPUT_GATE, "GATE"},
                     {M::OUTPUT_RR_UNI, "RR UNI"},
                     {M::OUTPUT_RR_BI, "RR BI"},
                 })
            {
                auto ot =
                    rack::createOutputCentered<SampleCreatorPort>(rack::Vec(c1, q), module, o);
                addOutput(ot);

                auto lab = OutPortLabel::createCentered(rack::Vec(c1, q + 17), priorBSx * 0.25, l);
                addChild(lab);

                c1 += priorBSx * 0.25;
            }
        }

        // OK so start aying out the midi section
        {
            auto x = 10;
            auto y = 60;
            int idx = 0;
            for (auto [o, l] : {std::make_pair(M::MIDI_START_RANGE, "Start Note"),
                                {M::MIDI_END_RANGE, "End Note"},
                                {M::MIDI_STEP_SIZE, "Step"},
                                {M::NUM_VEL_LAYERS, "Vel Layers"},
                                {M::NUM_ROUND_ROBINS, "Rnd Robins"},
                                {M::GATE_TIME, "Gate Time"}})
            {
                auto lw = PanelLabel::createCentered(rack::Vec(x + 30, y), 60, l);
                addChild(lw);
                auto k = rack::createParamCentered<PixelKnob<20>>(rack::Vec(x + 75, y), module, o);
                addChild(k);
                auto d = SCPanelParamDisplay::create(rack::Vec(x + 95, y), 50, module, o);
                addChild(d);

                y += 28;
                idx++;
                if (idx == 3)
                {
                    y = 60;
                    x = 10 + 95 + 50 + 5;
                }
            }
        }

        auto lwp = 230;
        auto vu =
            SampleCreatorVU::create(rack::Vec(10, lwp - 30), rack::Vec(box.size.x - 20, 25), m);
        addChild(vu);
        auto log = SampleCreatorLogWidget::create(
            rack::Vec(10, lwp), rack::Vec(box.size.x - 20, box.size.y - lwp - 65), m);
        addChild(log);

        {
            auto x = 10;
            auto y = 10;

            auto add = [&x, &y, this](auto lab, auto onc) {
                auto bt = SCPanelPushButton::create(rack::Vec(x, y), rack::Vec(60, 18), lab, onc);
                x += 63;
                addChild(bt);
            };

            add("Test", [m]() {
                if (m && m->createState == SampleCreatorModule::INACTIVE)
                {
                    m->testMode = true;
                    m->startOperating = true;
                }
            });
            add("Start", [m]() {
                {
                    m->testMode = false;
                    m->startOperating = true;
                }
            });
            add("Stop", [m]() {
                if (m)
                    m->stopImmediately = true;
            });
            add("Set Path", [this]() { selectPath(); });
        }
    }

    ~SampleCreatorModuleWidget() {}

    void selectPath()
    {
        auto scm = dynamic_cast<SampleCreatorModule *>(module);
        if (!scm)
            return;

        auto pt = scm->currentSampleDir;
        if (pt.empty())
        {
            pt = fs::path{rack::asset::userDir} / "SampleCreator";
            fs::create_directories(pt);
        }
        else
        {
            // Last time I was in "MySamples/Foo" and now I want to select somethign in MySamples
            pt = pt.parent_path();
        }

        char *path = osdialog_file(OSDIALOG_OPEN_DIR, pt.u8string().c_str(), "", NULL);
        if (path)
        {
            scm->currentSampleDir = fs::path{path};
            free(path);
        }
    }

    void appendContextMenu(rack::Menu *menu) override {}

    void drawBG(NVGcontext *vg)
    {
        auto cutPoint{58};

        // Main Gradient Background
        nvgBeginPath(vg);
        nvgFillPaint(vg, nvgLinearGradient(vg, 0, 50, 0, box.size.y - cutPoint,
                                           sampleCreatorSkin.panelGradientStart(),
                                           sampleCreatorSkin.panelGradientEnd()));

        nvgRect(vg, 0, 0, box.size.x, box.size.y - cutPoint);
        nvgFill(vg);
        nvgStroke(vg);

        // Draw the bottom region
        nvgBeginPath(vg);
        nvgFillColor(vg, sampleCreatorSkin.panelBottomRegion());
        nvgStrokeColor(vg, sampleCreatorSkin.panelBottomStroke());
        nvgStrokeWidth(vg, 0.5);
        nvgRect(vg, 0, box.size.y - cutPoint, box.size.x, cutPoint);
        nvgFill(vg);
        nvgStroke(vg);

        // Input region
        auto priorBSx = 10 * SCREW_WIDTH;
        auto fid = APP->window->loadFont(sampleCreatorSkin.fontPath)->handle;
        nvgBeginPath(vg);
        nvgStrokeColor(vg, sampleCreatorSkin.panelInputBorder());
        nvgFillColor(vg, sampleCreatorSkin.panelInputFill());
        nvgStrokeWidth(vg, 1);
        nvgRoundedRect(vg, 4, box.size.y - cutPoint + 2, priorBSx * 0.5 - 8, 38, 2);
        nvgFill(vg);
        nvgStroke(vg);

        auto dc = priorBSx * 0.11;

        // Output Region
        nvgBeginPath(vg);
        nvgStrokeColor(vg, sampleCreatorSkin.panelOutputBorder());
        nvgFillColor(vg, sampleCreatorSkin.panelOutputFill());
        nvgStrokeWidth(vg, 1);
        auto outw = box.size.x - priorBSx * 0.5 - 4;

        nvgRoundedRect(vg, priorBSx * 0.5, box.size.y - cutPoint + 2, outw, 38, 2);
        nvgFill(vg);
        nvgStroke(vg);

        fid = APP->window->loadFont(sampleCreatorSkin.fontPathMedium)->handle;
        nvgBeginPath(vg);
        nvgFillColor(vg, sampleCreatorSkin.panelBrandText());
        nvgTextAlign(vg, NVG_ALIGN_BOTTOM | NVG_ALIGN_CENTER);
        nvgFontFaceId(vg, fid);
        nvgFontSize(vg, 14);
        nvgText(vg, box.size.x * 0.5, box.size.y - 2, "SampleCreator", nullptr);

        // Outline the module
        nvgBeginPath(vg);
        nvgStrokeColor(vg, sampleCreatorSkin.moduleOutline());
        nvgStrokeWidth(vg, 1);
        nvgRect(vg, 0, 0, box.size.x, box.size.y);
        nvgStroke(vg);
    }

    void onSkinChanged() override { bg->dirty = true; }

    void step() override
    {
        if (module)
        {
        }

        sampleCreatorSkin.step();
        rack::ModuleWidget::step();
    }
};
} // namespace baconpaul::samplecreator

rack::Model *sampleCreatorModel =
    rack::createModel<baconpaul::samplecreator::SampleCreatorModule,
                      baconpaul::samplecreator::SampleCreatorModuleWidget>("SampleCreator");
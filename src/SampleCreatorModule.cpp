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
#include <compare>

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

struct SampleCreatorJobsKeyboard : rack::Widget, SampleCreatorSkin::Client
{
    SampleCreatorModule *module{nullptr};

    static SampleCreatorJobsKeyboard *create(const rack::Vec &pos, const rack::Vec &size,
                                             SampleCreatorModule *m)
    {
        auto res = new SampleCreatorJobsKeyboard();
        res->box.size = size;
        res->box.pos = pos;
        res->module = m;

        return res;
    }

    std::vector<SampleCreatorModule::RenderJob> jobs;
    int sn{0}, en{127};
    void pushNewJobSet(const std::vector<SampleCreatorModule::RenderJob> &j)
    {
        jobs = j;
        sn = 127;
        en = 0;
        for (auto j : jobs)
        {
            sn = std::min(j.noteFrom, sn);
            en = std::max(j.noteTo, en);
        }
        // round to octave
        sn = (sn / 12) * 12;
        en = ((en + 11) / 12) * 12;
    }

    void drawKeyboard(NVGcontext *vg)
    {

        auto mks = box.size.x / (en - sn);

        for (int i = sn; i < en; ++i)
        {
            auto k = i % 12;
            auto bk = false;
            if (k == 1 || k == 3 || k == 6 || k == 8 || k == 10)
                bk = true;
            nvgBeginPath(vg);
            nvgFillColor(vg, nvgRGB(100, 100, 100));
            nvgRect(vg, (i - sn) * mks, 0, mks, box.size.y);
            nvgFill(vg);
            if (bk)
            {
                nvgBeginPath(vg);
                nvgFillColor(vg, nvgRGB(40, 40, 40));
                nvgRect(vg, (i - sn) * mks, 0, mks, box.size.y * 0.7);
                nvgFill(vg);
            }

            if (i % 12 == 0)
            {
                nvgSave(vg);
                nvgBeginPath(vg);
                nvgFillColor(vg, nvgRGB(225, 225, 225));
                nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                nvgFontSize(vg, 7);
                float tx[6], tr[6];
                nvgTransformIdentity(tx);
                nvgTransformRotate(tx, M_PI * 0.5);
                nvgTransformIdentity(tr);
                nvgTransformTranslate(tr, (i - sn) * mks + mks / 2, box.size.y * 0.9);
                nvgTransformMultiply(tx, tr);

                nvgTransform(vg, tx[0], tx[1], tx[2], tx[3], tx[4], tx[5]);
                nvgText(vg, 0, 0, std::to_string(i).c_str(), nullptr);
                nvgRestore(vg);
            }
        }
    }

    void draw(const DrawArgs &args) override
    {
        auto vg = args.vg;

        drawKeyboard(vg);

        auto mks = box.size.x / (en - sn);
        auto vls = box.size.y / 128.0;

        auto idx = 0;
        auto cji = -1;
        if (module)
        {
            cji = module->currentJobIndex;
        }
        for (auto j : jobs)
        {
            auto mn = (j.midiNote - sn) * mks;
            auto ve = (127 - j.velocity) * vls;

            auto xs = (j.noteFrom - sn) * mks;
            auto xe = (j.noteTo - sn) * mks;
            auto ys = (127 - j.velTo) * vls;
            auto ye = (127 - j.velFrom) * vls;

            if (idx == cji)
            {
                // paint in the glow layer
            }
            else
            {
                nvgStrokeColor(vg, nvgRGB(220, 220, 220));
                if (idx > cji)
                {
                    nvgFillColor(vg, nvgRGBA(220, 220, 220, 100));
                }
                else
                {
                    nvgFillColor(vg, nvgRGBA(120, 120, 120, 120));
                }

                nvgBeginPath(vg);

                nvgRect(vg, xs, ys, xe - xs, ye - ys);
                nvgFill(vg);
                nvgStroke(vg);
            }
            idx++;
        }
    }

    void drawLayer(const DrawArgs &args, int layer) override
    {
        if (layer == 1)
        {
            auto vg = args.vg;
            auto mks = box.size.x / (en - sn);
            auto vls = box.size.y / 128.0;

            auto idx = 0;
            auto cji = -1;
            if (module)
            {
                cji = module->currentJobIndex;
            }
            for (auto j : jobs)
            {
                auto mn = (j.midiNote - sn) * mks;
                auto ve = (127 - j.velocity) * vls;

                auto xs = (j.noteFrom - sn) * mks;
                auto xe = (j.noteTo - sn) * mks;
                auto ys = (127 - j.velTo) * vls;
                auto ye = (127 - j.velFrom) * vls;

                if (idx == cji)
                {
                    nvgStrokeColor(vg, nvgRGB(220, 220, 255));
                    nvgFillColor(vg, nvgRGBA(220, 220, 255, 200));

                    nvgBeginPath(vg);
                    nvgRect(vg, xs, ys, xe - xs, ye - ys);
                    nvgFill(vg);
                    nvgStroke(vg);
                }
                idx++;

                nvgBeginPath(vg);
                nvgStrokeColor(vg, nvgRGB(255, 255, 0));
                nvgEllipse(vg, mn, ve, 1, 1);
                nvgStroke(vg);
            }
        }
    }

    void onSkinChanged() override {}
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
        box.size = rack::Vec(SCREW_WIDTH * 30, RACK_HEIGHT);
        setupPositions();

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
            tw->box.pos = rack::Vec(box.size.x * 0.5 - 65, box.size.y - 20);
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
            tw->box.pos = rack::Vec(box.size.x * 0.5 + 48, box.size.y - 15);
            tw->scale(0.35);
            tw->addChild(sOm);

            addChild(tw);
        }

        // Input Elements
        auto startFrom = [](auto &r) {
            auto ps = r;
            ps.pos.x += 18;
            ps.pos.y += 14;
            auto pl = ps;
            pl.pos.y += 18;
            return std::make_pair(pl, ps);
        };

        int portdX{34};

        {
            auto [pl, ps] = startFrom(inputRegion);

            auto inl = rack::createInputCentered<SampleCreatorPort>(ps.pos, module, M::INPUT_L);
            inl->connectAsInputFromMixmaster = true;
            inl->mixMasterStereoCompanion = M::INPUT_R;

            auto lab = InPortLabel ::createCentered(pl.pos, portdX, "L");
            addChild(lab);

            ps.pos.x += portdX;
            pl.pos.x += portdX;

            auto inr = rack::createInputCentered<SampleCreatorPort>(ps.pos, module, M::INPUT_R);
            inr->connectAsInputFromMixmaster = true;
            inr->mixMasterStereoCompanion = M::INPUT_L;

            lab = InPortLabel::createCentered(pl.pos, portdX, "R");
            addChild(lab);

            pl.pos.x -= portdX / 2;
            lab = InPortLabel::createCentered(pl.pos, portdX, "In");
            addChild(lab);

            addInput(inl);
            addInput(inr);

            auto vu = SampleCreatorVU::create(
                rack::Vec(ps.pos.x + portdX / 2, inputRegion.pos.y + 5),
                rack::Vec(inputRegion.pos.x + inputRegion.size.x - ps.pos.x - 15 - 8,
                          inputRegion.size.y - 23),
                m);
            addChild(vu);

            pl.pos.x += portdX * 1.4;
            lab = InPortLabel::createCentered(pl.pos, portdX, "Level");
            addChild(lab);
        }

        // outputs
        {
            auto [pl, ps] = startFrom(outputRegion);

            for (auto [o, l] : {
                     std::make_pair(M::OUTPUT_VOCT, "V/Oct"),
                     {M::OUTPUT_VELOCITY, "Vel"},
                     {M::OUTPUT_GATE, "Gate"},
                     {M::OUTPUT_RR_ONE, "RR 1"},
                     {M::OUTPUT_RR_TWO, "RR 2"},
                 })
            {
                auto outL = rack::createOutputCentered<SampleCreatorPort>(ps.pos, module, o);
                auto lab = OutPortLabel ::createCentered(pl.pos, portdX, l);
                addOutput(outL);
                addChild(lab);

                ps.pos.x += portdX;
                pl.pos.x += portdX;
            }
            auto rrRegion = outputRegion;
            rrRegion.pos.y += 42;
            rrRegion.size.y -= 42;

            auto fh = rrRegion;
            fh.size.x *= 0.5;

            auto addRR = [&](auto l, auto p) {
                auto lp = fh;
                auto dp = fh;
                lp.size.x = 25;
                dp.pos.x += 25;
                dp.size.x -= 25;
                dp = dp.grow({-margin, -margin});
                addChild(OutPortLabel::create(lp, l));
                addChild(SCPanelDropDown::create(dp.pos, dp.size, m, p));
                fh.pos.x += fh.size.x;
            };

            addRR("RR1", M::RR1_TYPE);
            addRR("RR2", M::RR2_TYPE);
        }

        jobsKeyboard = SampleCreatorJobsKeyboard::create(
            rack::Vec(margin, margin),
            rack::Vec(box.size.x - 2 * margin, keyboardYEnd - 2 * margin), m);
        addChild(jobsKeyboard);

        auto rangeSubRegion = rangeRegion;
        rangeSubRegion.size.y = rangeRegion.size.y * 0.2;

        auto rangePos = [](auto &subr) {
            auto fh = subr;
            fh.size.x = subr.size.x * 0.5;
            auto sh = subr;
            sh.pos.x += subr.size.x * 0.5;
            sh.size.x -= subr.size.x * 0.5;

            auto lh = 12;
            auto fhl = fh;
            auto fhw = fh;
            fhl.size.y = lh;
            fhw.pos.y += lh;
            fhw.size.y -= lh;

            auto shl = sh;
            auto shw = sh;
            shl.size.y = lh;
            shw.pos.y += lh;
            shw.size.y -= lh;

            return std::make_tuple(fhl, fhw.shrink({3, 3}), shl, shw.shrink({3, 3}));
        };

        auto pWithK = [this](const auto &rect, int id) {
            auto kctr = rect.pos;
            kctr.x += rect.size.y * 0.5;
            kctr.y += rect.size.y * 0.5;
            addParam(rack::createParamCentered<PixelKnob<16>>(kctr, module, id));
            auto bctr = rect;
            bctr.pos.x += 20;
            bctr.size.x -= 20;
            addChild(SCPanelParamDisplay::create(bctr, module, id));
        };

        {
            auto [fl, fw, sl, sw] = rangePos(rangeSubRegion);
            rangeSubRegion.pos.y += rangeSubRegion.size.y;

            addChild(OutPortLabel::createCtrlLabel(fl, "Start"));
            pWithK(fw, M::MIDI_START_RANGE);

            addChild(OutPortLabel::createCtrlLabel(sl, "End"));
            pWithK(sw, M::MIDI_END_RANGE);
        }

        {
            auto [fl, fw, sl, sw] = rangePos(rangeSubRegion);
            rangeSubRegion.pos.y += rangeSubRegion.size.y;

            addChild(OutPortLabel::createCtrlLabel(fl, "Gate Time"));
            pWithK(fw, M::GATE_TIME);

            addChild(OutPortLabel::createCtrlLabel(sl, "Release Mode"));
            addChild(SCPanelDropDown::create(sw.pos, sw.size, module, M::REL_MODE));
        }

        {
            auto [fl, fw, sl, sw] = rangePos(rangeSubRegion);
            rangeSubRegion.pos.y += rangeSubRegion.size.y;

            addChild(OutPortLabel::createCtrlLabel(fl, "Step Size"));
            pWithK(fw, M::MIDI_STEP_SIZE);

            addChild(OutPortLabel::createCtrlLabel(sl, "Round Robin"));
            pWithK(sw, M::NUM_ROUND_ROBINS);
        }

        {
            auto [fl, fw, sl, sw] = rangePos(rangeSubRegion);
            rangeSubRegion.pos.y += rangeSubRegion.size.y;

            addChild(OutPortLabel::createCtrlLabel(fl, "Vel Layers"));
            pWithK(fw, M::NUM_VEL_LAYERS);

            addChild(OutPortLabel::createCtrlLabel(sl, "Vel Strategy"));
            addChild(SCPanelDropDown::create(sw.pos, sw.size, module, M::VELOCITY_STRATEGY));
        }

        {
            auto [fl, fw, sl, sw] = rangePos(rangeSubRegion);
            rangeSubRegion.pos.y += rangeSubRegion.size.y;

            addChild(OutPortLabel::createCtrlLabel(fl, "Latency"));
            pWithK(fw, M::LATENCY_COMPENSATION);

            addChild(OutPortLabel::createCtrlLabel(sl, "Polyphony"));
            pWithK(sw, M::POLYPHONY);
        }

        auto log = SampleCreatorLogWidget::create(logRegion.pos, logRegion.size, m);
        addChild(log);

        {
            auto rg = statusRegion;
            rg.size.x = rg.size.y;

            auto add = [&rg, this](auto lab, auto onc) {
                auto ug = rg.shrink({margin, margin});
                auto bt = SCPanelPushButton::create(ug.pos, ug.size, lab, onc);
                addChild(bt);
                rg.pos.x += rg.size.x;
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
        }

        {
            auto ug = pathRegion.shrink({margin, margin});
            auto bt =
                SCPanelPushButton::create(ug.pos, ug.size, "Set Path", [this]() { selectPath(); });
            addChild(bt);
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

    int footerHeight{18};
    int keyboardYEnd{0}, controlsYEnd{0};
    int controlsXSplit{0};
    float rectCorner{2};
    float margin{1.4};

    rack::Rect inputRegion, outputRegion, rangeRegion, logRegion, statusRegion, pathRegion;

    SampleCreatorJobsKeyboard *jobsKeyboard{nullptr};

    void setupPositions()
    {
        auto regionHeight = box.size.y - footerHeight;

        keyboardYEnd = regionHeight * 0.2;
        controlsYEnd = regionHeight * 0.71;
        controlsXSplit = box.size.x * 0.61;

        auto inpHeight = regionHeight * 0.115;
        inputRegion =
            rack::Rect(controlsXSplit, controlsYEnd, box.size.x - controlsXSplit, inpHeight)
                .grow({-margin, -margin});
        outputRegion =
            rack::Rect(controlsXSplit, controlsYEnd + inpHeight, box.size.x - controlsXSplit,
                       regionHeight - controlsYEnd - inpHeight)
                .grow({-margin, -margin});
        rangeRegion = rack::Rect(controlsXSplit, keyboardYEnd, box.size.x - controlsXSplit,
                                 controlsYEnd - keyboardYEnd)
                          .grow({-margin, -margin});
        logRegion = rack::Rect(0, regionHeight * 0.6, controlsXSplit, regionHeight * 0.4)
                        .grow({-margin, -margin});
        statusRegion = rack::Rect(0, keyboardYEnd, controlsXSplit, regionHeight * 0.15)
                           .grow({-margin, -margin});
        pathRegion = statusRegion;
        pathRegion.pos.y += pathRegion.size.y;
    }

    void drawBG(NVGcontext *vg)
    {
        // Main Gradient Background
        nvgBeginPath(vg);
        nvgFillPaint(vg, nvgLinearGradient(vg, 0, 50, 0, box.size.y - footerHeight,
                                           sampleCreatorSkin.panelGradientStart(),
                                           sampleCreatorSkin.panelGradientEnd()));

        nvgRect(vg, 0, 0, box.size.x, box.size.y - footerHeight);
        nvgFill(vg);
        nvgStroke(vg);

        // Draw the bottom region
        nvgBeginPath(vg);
        nvgFillColor(vg, sampleCreatorSkin.panelBottomRegion());
        nvgStrokeColor(vg, sampleCreatorSkin.panelBottomStroke());
        nvgStrokeWidth(vg, 0.5);
        nvgRect(vg, 0, box.size.y - footerHeight, box.size.x, footerHeight);
        nvgFill(vg);
        nvgStroke(vg);

        auto fid = APP->window->loadFont(sampleCreatorSkin.fontPathMedium)->handle;
        nvgBeginPath(vg);
        nvgFillColor(vg, sampleCreatorSkin.panelBrandText());
        nvgTextAlign(vg, NVG_ALIGN_BOTTOM | NVG_ALIGN_CENTER);
        nvgFontFaceId(vg, fid);
        nvgFontSize(vg, 14);
        nvgText(vg, box.size.x * 0.5, box.size.y - 2, "SampleCreator", nullptr);

        // Input region
        nvgBeginPath(vg);
        nvgStrokeColor(vg, sampleCreatorSkin.panelInputBorder());
        nvgFillColor(vg, sampleCreatorSkin.panelInputFill());
        nvgStrokeWidth(vg, 1);
        nvgRoundedRect(vg, inputRegion.pos.x, inputRegion.pos.y, inputRegion.size.x,
                       inputRegion.size.y, rectCorner);
        nvgFill(vg);
        nvgStroke(vg);

        // Output region
        nvgBeginPath(vg);
        nvgStrokeColor(vg, sampleCreatorSkin.panelOutputBorder());
        nvgFillColor(vg, sampleCreatorSkin.panelOutputFill());
        nvgStrokeWidth(vg, 1);
        nvgRoundedRect(vg, outputRegion.pos.x, outputRegion.pos.y, outputRegion.size.x,
                       outputRegion.size.y, rectCorner);
        nvgFill(vg);
        nvgStroke(vg);

        // Control Region
        nvgBeginPath(vg);
        nvgStrokeColor(vg, sampleCreatorSkin.panelControlBorder());
        nvgFillColor(vg, sampleCreatorSkin.panelControlFill());
        nvgStrokeWidth(vg, 1);
        nvgRoundedRect(vg, rangeRegion.pos.x, rangeRegion.pos.y, rangeRegion.size.x,
                       rangeRegion.size.y, rectCorner);
        nvgFill(vg);
        nvgStroke(vg);

#if 0
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

        // Outline the module
        nvgBeginPath(vg);
        nvgStrokeColor(vg, sampleCreatorSkin.moduleOutline());
        nvgStrokeWidth(vg, 1);
        nvgRect(vg, 0, 0, box.size.x, box.size.y);
        nvgStroke(vg);
#endif
    }

    void onSkinChanged() override { bg->dirty = true; }

    float cacheParamVals[M::NUM_PARAMS]{-100000}; // make midi start note bad so the step fires
    void step() override
    {
        if (module)
        {
            bool paramChanged = false;
            for (const auto &invals :
                 {M::MIDI_START_RANGE, M::MIDI_END_RANGE, M::MIDI_STEP_SIZE, M::NUM_VEL_LAYERS,
                  M::NUM_ROUND_ROBINS, M::VELOCITY_STRATEGY})
            {
                if (cacheParamVals[invals] != module->getParamQuantity(invals)->getValue())
                {
                    paramChanged = true;
                    cacheParamVals[invals] = module->getParamQuantity(invals)->getValue();
                }
            }

            if (paramChanged && jobsKeyboard)
            {
                std::vector<SampleCreatorModule::RenderJob> jobs;
                auto scm = dynamic_cast<SampleCreatorModule *>(module);
                assert(scm);
                if (scm)
                {
                    scm->populateRenderJobs(jobs);
                    for (auto j : jobs)
                    {
                        jobsKeyboard->pushNewJobSet(jobs);
                    }
                }
            }
        }

        sampleCreatorSkin.step();
        rack::ModuleWidget::step();
    }
};
} // namespace baconpaul::samplecreator

rack::Model *sampleCreatorModel =
    rack::createModel<baconpaul::samplecreator::SampleCreatorModule,
                      baconpaul::samplecreator::SampleCreatorModuleWidget>("SampleCreator");
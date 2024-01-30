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
#include <array>
#include <vector>
#include <memory>
#include <atomic>
#include <cstdint>
#include <cstdio>

#include <ghc/filesystem.hpp>
namespace fs = ghc::filesystem;

#include <tinywav.h>

#include "sst/cpputils/ring_buffer.h"

#include "sst/rackhelpers/json.h"
#include "sst/rackhelpers/ui.h"
#include "sst/rackhelpers/neighbor_connectable.h"

#include "SampleCreatorSkin.hpp"
#include "CustomWidgets.hpp"

#define MAX_POLY 16

namespace baconpaul::samplecreator
{

struct MidiNoteParamQuantity : rack::ParamQuantity
{
    std::vector<std::string> notes{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    std::string getDisplayValueString() override
    {
        auto v = (int)std::round(getValue());
        auto oct = v / 12 - 1;
        auto nt = v % 12;

        char res[256];
        snprintf(res, 256, "%s%d (%d)", notes[nt].c_str(), oct, v);
        return std::string(res);
    }
    void setDisplayValueString(std::string s) override { ParamQuantity::setDisplayValueString(s); }
};

struct SampleCreatorModule : virtual rack::Module,
                             sst::rackhelpers::module_connector::NeighborConnectable_V1
{
    enum ParamIds
    {
        MIDI_START_RANGE,
        MIDI_END_RANGE,
        MIDI_STEP_SIZE,

        NUM_PARAMS
    };

    enum InputIds
    {
        INPUT_L,
        INPUT_R,

        INPUT_GO,

        NUM_INPUTS
    };

    enum OutputIds
    {
        OUTPUT_VOCT,
        OUTPUT_GATE,
        OUTPUT_VELOCITY,

        NUM_OUTPUTS
    };

    enum LightIds
    {
        NUM_LIGHTS
    };

    SampleCreatorModule()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        configInput(INPUT_L, "Left / Mono Input");
        configInput(INPUT_R, "Right Input");
        configInput(INPUT_GO, "Start the process");

        configOutput(OUTPUT_VOCT, "VOct Output");
        configOutput(OUTPUT_GATE, "Gate Output");
        configOutput(OUTPUT_VELOCITY, "Velocity Output");

        auto p =
            configParam<MidiNoteParamQuantity>(MIDI_START_RANGE, 0, 127, 48, "MIDI Start Range");
        p->snapEnabled = true;

        p = configParam<MidiNoteParamQuantity>(MIDI_END_RANGE, 0, 127, 72, "MIDI Start Range");
        p->snapEnabled = true;

        auto q = configParam(MIDI_STEP_SIZE, 1, 24, 4, "MIDI Step Size");
        q->snapEnabled = true;
    }

    struct message_t
    {
        static constexpr size_t slen{256};
        char msg[slen]{};
        message_t() { msg[0] = 0; }
        message_t(message_t &&other) { strncpy(msg, other.msg, slen); }
        message_t(message_t &other) { strncpy(msg, other.msg, slen); }
        message_t operator=(const message_t &other)
        {
            strncpy(msg, other.msg, slen);
            return *this;
        }
    };
    sst::cpputils::SimpleRingBuffer<message_t, 16> messageBuffer;
    void pushMessage(const char *msg)
    {
        // fixme
        std::cout << msg << std::endl;
    }

    std::optional<std::vector<labeledStereoPort_t>> getPrimaryInputs() override
    {
        return {{std::make_pair("Input", std::make_pair(INPUT_L, INPUT_R))}};
    }

    json_t *dataToJson() override
    {
        auto res = json_object();
        return res;
    }

    void dataFromJson(json_t *rootJ) override { namespace jh = sst::rackhelpers::json; }

    static constexpr int wavBlockSize{256};
    float wavBlock[2][wavBlockSize]{};
    std::size_t wavBlockPosition{0};
    uint32_t noteNumber{0};
    uint64_t playbackPos{0};

    enum CreateState
    {
        INACTIVE,
        NEW_NOTE,
        GATED_RECORD,
        RELEASE_RECORD,
        SPINDOWN_BUFFER,
    } createState{INACTIVE};

    fs::path currentSampleDir{};
    TinyWav tinyWavControl;

    void process(const ProcessArgs &args) override
    {
        if (createState == INACTIVE && (inputs[INPUT_GO].getVoltage() > 2))
        {
            pushMessage("Starting Up");
            createState = NEW_NOTE;
            wavBlockPosition = 0;
            noteNumber = 60;

            currentSampleDir = fs::path{rack::asset::userDir} / "SampleCreator";
            fs::create_directories(currentSampleDir);
        }

        outputs[OUTPUT_VOCT].setVoltage(std::clamp((float)noteNumber / 12.f - 5.f, -5.f, 5.f));
        outputs[OUTPUT_GATE].setVoltage((createState == GATED_RECORD) * 10.f);

        if (createState == INACTIVE)
        {
            return;
        }

        if (createState == NEW_NOTE)
        {
            char ms[256];
            snprintf(ms, 256, "Starting note %d", noteNumber);
            pushMessage(ms);
            playbackPos = 0;
            createState = GATED_RECORD;

            auto fn = currentSampleDir / ("sample_midi_" + std::to_string(noteNumber) + ".wav");
            auto res = tinywav_open_write(&tinyWavControl, 2, args.sampleRate, TW_FLOAT32,
                                          TW_INTERLEAVED, fn.u8string().c_str());
        }

        if (playbackPos > args.sampleRate && createState == GATED_RECORD)
        {
            pushMessage("Releasing");
            createState = RELEASE_RECORD;
            playbackPos = 0;
        }

        if (playbackPos > args.sampleRate && createState == RELEASE_RECORD)
        {
            pushMessage("Done");
            createState = SPINDOWN_BUFFER;
            playbackPos = 0;
            tinywav_close_write(&tinyWavControl);
        }

        if (createState != SPINDOWN_BUFFER)
        {
            float d[2];
            d[0] = inputs[INPUT_L].getVoltage();
            d[1] = inputs[INPUT_R].getVoltage();

            // Insanely inefficient
            tinywav_write_f(&tinyWavControl, d, 1);
        }

        if (playbackPos > 1000 && createState == SPINDOWN_BUFFER)
        {
            pushMessage("Spindown Over");
            noteNumber++;
            if (noteNumber > 72)
            {
                createState = INACTIVE;
                pushMessage("Finished");
            }
            else
            {
                createState = NEW_NOTE;
                pushMessage("New Note");
            }
        }

        playbackPos++;
    }
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
            auto Omsvg = APP->window->loadSvg(
                rack::asset::plugin(pluginInstance, "res/BaconLogo.svg"));
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

            auto y = 20;
            auto x = 20;
            auto in =
                rack::createInputCentered<SampleCreatorPort>(rack::Vec(x, y), module, M::INPUT_GO);
            addInput(in);

            for (auto [o, l] : {std::make_pair(M::OUTPUT_VOCT, "V/OCT"),
                                {M::OUTPUT_VELOCITY, "VEL"},
                                {M::OUTPUT_GATE, "GATE"}})
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
            for (auto [o, l] : {std::make_pair(M::MIDI_START_RANGE, "Start Note"),
                                {M::MIDI_END_RANGE, "End Note"},
                                {M::MIDI_STEP_SIZE, "Step"}})
            {
                auto lw = PanelLabel::createCentered(rack::Vec(x + 30, y), 60, l);
                addChild(lw);
                auto k = rack::createParamCentered<PixelKnob<20>>(rack::Vec(x + 75, y), module, o);
                addChild(k);
                auto d = SCPanelParamDisplay::create(rack::Vec(x + 95, y), 50, module, o);
                addChild(d);

                y += 28;
            }
        }
    }

    ~SampleCreatorModuleWidget() {}

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
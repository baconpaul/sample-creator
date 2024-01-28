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
#include "sst/rackhelpers/module_connector.h"

#include "SampleCreatorSkin.hpp"
#include "CustomWidgets.hpp"

#define MAX_POLY 16

namespace baconpaul::samplecreator
{
struct SampleCreatorModule : virtual rack::Module,
                             sst::rackhelpers::module_connector::NeighborConnectable_V1
{
    enum ParamIds
    {
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

struct SampleCreatorPort
    : public sst::rackhelpers::module_connector::PortConnectionMixin<rack::app::SvgPort>
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

    SampleCreatorSkin::Skin lastSkin{SampleCreatorSkin::DARK};
    void step() override
    {
        bool dirty{false};
        if (lastSkin != sampleCreatorSkin.skin)
            setPortGraphics();
        lastSkin = sampleCreatorSkin.skin;

        rack::Widget::step();
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

        int headerSize{38};

        // Input Elements
        {
            auto priorBSx = 10 * SCREW_WIDTH;
            auto q = RACK_HEIGHT - 42;
            auto c1 = priorBSx * 0.25;
            auto dc = priorBSx * 0.11;
            auto c2 = priorBSx * 0.75;
            auto inl = rack::createInputCentered<SampleCreatorPort>(rack::Vec(c1 - dc, q), module,
                                                                    M::INPUT_L);
            inl->connectAsInputFromMixmaster = true;
            inl->mixMasterStereoCompanion = M::INPUT_R;
            auto inr = rack::createInputCentered<SampleCreatorPort>(rack::Vec(c1 + dc, q), module,
                                                                    M::INPUT_R);
            inr->connectAsInputFromMixmaster = true;
            inr->mixMasterStereoCompanion = M::INPUT_L;

            addInput(inl);
            addInput(inr);
        }

        {
            auto y = 50;
            auto x = 40;
            auto in =
                rack::createInputCentered<SampleCreatorPort>(rack::Vec(x, y), module, M::INPUT_GO);
            addInput(in);

            y += 40;
            for (auto o : {M::OUTPUT_VOCT, M::OUTPUT_VELOCITY, M::OUTPUT_GATE})
            {
                auto ot = rack::createOutputCentered<SampleCreatorPort>(rack::Vec(x, y), module, o);
                addOutput(ot);
                y += 40;
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
        nvgRoundedRect(vg, 4, box.size.y - cutPoint + 3, priorBSx * 0.5 - 8, 37, 2);
        nvgFill(vg);
        nvgStroke(vg);

        auto dc = priorBSx * 0.11;

        nvgBeginPath(vg);
        nvgFillColor(vg, sampleCreatorSkin.panelInputText());
        nvgTextAlign(vg, NVG_ALIGN_BOTTOM | NVG_ALIGN_CENTER);
        nvgFontFaceId(vg, fid);
        nvgFontSize(vg, 10);
        nvgText(vg, priorBSx * 0.25, box.size.y - cutPoint + 38, "IN", nullptr);

        nvgFontSize(vg, 10);
        nvgText(vg, priorBSx * 0.25 - dc, box.size.y - cutPoint + 38, "L", nullptr);
        nvgText(vg, priorBSx * 0.25 + dc, box.size.y - cutPoint + 38, "R", nullptr);

        // Output Region
        nvgBeginPath(vg);
        nvgStrokeColor(vg, sampleCreatorSkin.panelOutputBorder());
        nvgFillColor(vg, sampleCreatorSkin.panelOutputFill());
        nvgStrokeWidth(vg, 1);
        auto outw = box.size.x - priorBSx * 0.5 - 4;

        nvgRoundedRect(vg, priorBSx * 0.5, box.size.y - cutPoint + 3, outw, 37, 2);
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
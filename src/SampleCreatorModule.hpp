/*
 * SampleCreator
 *
 * An experimental idea based on a preliminary convo. Probably best to come back later.
 *
 * Copyright Paul Walker 2024
 *
 * Released under the MIT License. See `LICENSE.md` for details
 */

#ifndef SRC_SAMPLECREATORMODULE_HPP
#define SRC_SAMPLECREATORMODULE_HPP

#include <array>
#include <vector>
#include <memory>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <random>
#include <chrono>
#include <thread>
#include <fstream>

#include <rack.hpp>

#include <ghc/filesystem.hpp>
namespace fs = ghc::filesystem;

#include <sst/cpputils/ring_buffer.h>

#include <sst/rackhelpers/json.h>
#include <sst/rackhelpers/neighbor_connectable.h>

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

        NUM_VEL_LAYERS,
        NUM_ROUND_ROBINS,

        GATE_TIME,

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

        OUTPUT_RR_UNI,
        OUTPUT_RR_BI,

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
        configOutput(OUTPUT_RR_UNI, "Unipolar (0-10v) Uniform RR CV");
        configOutput(OUTPUT_RR_BI, "Bipolar (+/-5v) Uniform RR CV");

        auto p =
            configParam<MidiNoteParamQuantity>(MIDI_START_RANGE, 0, 127, 48, "MIDI Start Range");
        p->snapEnabled = true;

        p = configParam<MidiNoteParamQuantity>(MIDI_END_RANGE, 0, 127, 72, "MIDI Start Range");
        p->snapEnabled = true;

        auto q = configParam(MIDI_STEP_SIZE, 1, 24, 4, "MIDI Step Size");
        q->snapEnabled = true;

        {
            auto q = configParam(NUM_VEL_LAYERS, 1, 8, 1, "Velocity Layers");
            q->snapEnabled = true;
        }

        {
            auto q = configParam(NUM_ROUND_ROBINS, 1, 16, 1, "Round Robins");
            q->snapEnabled = true;
        }

        configParam(GATE_TIME, 0.2, 16, 1, "Gate Time", "s");

        renderThread = std::make_unique<std::thread>([this]() { renderThreadProcess(); });

        pushMessage("Sample Creator Started");
    }

    ~SampleCreatorModule()
    {
        keepRunning = false;
        renderThread->join();
    }

    std::default_random_engine reng;
    std::uniform_real_distribution<float> uniReal{0.f, 1.f};

    // tis is not entirely thread safe and strings can allocate but
    // it is infrequenty used. Good enough for now.
    std::array<std::string, 1024> messageBuffer;
    std::atomic<int32_t> mbWrite{0}, mbRead{0};
    void pushMessage(const std::string &s)
    {
        messageBuffer[mbWrite] = s;
        mbWrite = (mbWrite + 1) & 1023;
    }
    bool hasMessage() { return mbRead != mbWrite; }
    std::string popMessage()
    {
        auto res = messageBuffer[mbRead];
        mbRead = (mbRead + 1) & 1023;
        return res;
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

    riffwav::RIFFWavWriter riffWavWriter;
    std::ofstream sfzFile;

    std::atomic<bool> testMode{false};
    std::atomic<bool> startOperating{false};
    std::atomic<bool> stopImmediately{false};

    std::array<std::atomic<float>, 2> vuLevels{0, 0};

    uint64_t gateSamples;

    static constexpr int silenceSamples{4096};
    int silencePosition;
    float silenceDetector[silenceSamples];

    struct RenderJob
    {
        int midiNote, noteFrom, noteTo;
        int velocity{90}, velFrom, velTo;
        int roundRobinIndex{0};
        int roundRobinOutOf{1};
        float rrRand[2];
    };

    std::vector<RenderJob> renderJobs;
    int64_t currentJobIndex{-1}; // don't need to be atomic since only used on audio thread

    static constexpr int ioSampleBlockSize{16};
    static constexpr int ioSampleBlocksAvailable{8192};
    float ioBlocks[ioSampleBlocksAvailable][ioSampleBlockSize][2];
    std::atomic<int> ioWriteBlock{0},
        ioWritePosition{0}; // don't need to be atomic since only used on audio thread

    std::unique_ptr<std::thread> renderThread;
    std::atomic<bool> keepRunning{true};

    struct RenderThreadCommand
    {
        enum Message
        {
            START_RENDER,
            END_RENDER,
            NEW_NOTE, // data is a job index
            CLOSE_FILE,
            PUSH_SAMPLES // terrible implementation right now
        } message;

        int64_t data{0};
        int64_t data2{0};
    };
    sst::cpputils::SimpleRingBuffer<RenderThreadCommand, 4096 * 16> renderThreadCommands;
    void renderThreadProcess()
    {
        while (keepRunning)
        {
            while (keepRunning && !renderThreadCommands.empty())
            {
                auto oc = renderThreadCommands.pop();
                if (oc.has_value())
                {
                    switch (oc->message)
                    {
                    case RenderThreadCommand::START_RENDER:
                    {
                        if (!testMode)
                        {
                            auto fn = currentSampleDir / "sample.sfz";
                            sfzFile = std::ofstream(fn);
                            sfzFile << "// Basic SFZ File from Rack Sample Creator\n\n";
                            sfzFile << "<global>\n" << std::flush;
                        }
                    }
                    break;
                    case RenderThreadCommand::END_RENDER:
                    {
                        pushMessage("END RENDER");
                        if (!testMode)
                        {
                            pushMessage("Closing SFZ File");
                            if (sfzFile.is_open())
                            {
                                pushMessage("Which is open");
                                sfzFile.close();
                            }
                        }
                    }
                    break;
                    case RenderThreadCommand::NEW_NOTE:
                        renderThreadNewNote(oc->data, oc->data2);
                        break;
                    case RenderThreadCommand::CLOSE_FILE:
                        riffWavWriter.closeFile();
                        break;
                    case RenderThreadCommand::PUSH_SAMPLES:
                    {
                        updateVU(oc->data);
                        renderThreadWriteBlock(oc->data);
                    }
                    break;
                    }
                }
            }
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(20ms);
        }
    }

    void renderThreadNewNote(int jobid, double sr)
    {
        auto &currentJob = renderJobs[jobid];
        pushMessage(std::string("Starting note ") + std::to_string(currentJob.midiNote));

        auto bn = std::string("sample") + "_note_" + std::to_string((int)currentJob.midiNote) +
                  "_vel_" + std::to_string((int)currentJob.velocity) + "_rr_" +
                  std::to_string((int)currentJob.roundRobinIndex) + ".wav";
        auto fn = currentSampleDir / bn;
        if (!testMode)
        {
            int nChannels = inputs[INPUT_R].isConnected() ? 2 : 1;
            pushMessage(std::string("Writing ") + (nChannels == 2 ? "stereo" : "mono") + " file '" +
                        fn.filename().u8string() + "' at 32-bit " + std::to_string(sr) + " sr");
            riffWavWriter = riffwav::RIFFWavWriter(fn, nChannels);
            riffWavWriter.openFile();
            riffWavWriter.writeRIFFHeader();
            riffWavWriter.writeFMTChunk(sr);
            riffWavWriter.writeINSTChunk(currentJob.midiNote, currentJob.noteFrom,
                                         currentJob.noteTo, currentJob.velFrom, currentJob.velTo);
            riffWavWriter.startDataChunk();

            if (currentJob.roundRobinIndex == 0)
            {
                sfzFile << "\n<group> "
                        << " seq_length=" << currentJob.roundRobinOutOf << "\n"
                        << std::flush;
            }

            sfzFile << "<region>seq_position=" << (currentJob.roundRobinIndex + 1)
                    << " sample=" << fn.filename().u8string().c_str()
                    << " lokey=" << currentJob.noteFrom << " hikey=" << currentJob.noteTo
                    << " pitch_keycenter=" << currentJob.midiNote << " lovel=" << currentJob.velFrom
                    << " hivel=" << currentJob.velTo << "\n"
                    << std::flush;
        }
    }

    void renderThreadWriteBlock(int whichBlock)
    {
        if (testMode)
            return;
        auto *data = &(ioBlocks[whichBlock][0][0]);
        if (riffWavWriter.nChannels == 2)
        {
            riffWavWriter.pushInterleavedBlock(data, ioSampleBlockSize * 2);
        }
        if (riffWavWriter.nChannels == 1)
        {
            float md[ioSampleBlockSize];
            for (int i = 0; i < ioSampleBlockSize; ++i)
            {
                md[i] = data[i * 2];
            }
            riffWavWriter.pushInterleavedBlock(data, ioSampleBlockSize);
        }
    }

    void populateRenderJobs(std::vector<RenderJob> &onto)
    {
        onto.clear();
        auto numVel = (int)std::round(getParam(NUM_VEL_LAYERS).getValue());
        auto numRR = (int)std::round(getParam(NUM_ROUND_ROBINS).getValue());
        auto midiStep = (int)std::round(getParam(MIDI_STEP_SIZE).getValue());
        auto midiHalf = midiStep <= 2 ? 0 : midiStep / 2;

        auto midiStart = (int)std::round(getParam(MIDI_START_RANGE).getValue());
        auto midiEnd = (int)std::round(getParam(MIDI_END_RANGE).getValue());

        onto.clear();
        for (int mn = midiStart + midiHalf; mn <= midiEnd; mn += midiStep)
        {
            auto nf = mn - midiHalf;
            auto nt = mn + midiStep - 1 - (midiStep > 2);
            if (mn + midiStep > midiEnd)
                nt = midiEnd;

            auto dVel = 1.0 / (numVel);

            RenderJob mrj;
            mrj.midiNote = mn;
            mrj.noteFrom = nf;
            mrj.noteTo = nt;

            for (int vl = 0; vl < numVel; ++vl)
            {
                auto bv = (vl + 0.5) * dVel;
                auto sv = vl * dVel;
                auto ev = (vl + 1) * dVel;
                auto mv = (int)std::round(std::clamp(sqrt(bv), 0., 1.) * 127);
                auto msv = (int)std::round(std::clamp(sqrt(sv), 0., 1.) * 127);
                auto mev = (int)std::round(std::clamp(sqrt(ev), 0., 1.) * 127);

                auto vrj = mrj;
                vrj.velocity = mv;
                vrj.velFrom = msv;
                vrj.velTo = mev;

                for (int rr = 0; rr < numRR; ++rr)
                {
                    auto rj = vrj;
                    rj.roundRobinIndex = rr;
                    rj.roundRobinOutOf = numRR;
                    rj.rrRand[0] = uniReal(reng) * 10.f;
                    rj.rrRand[1] = uniReal(reng) * 10.f - 5.f;
                    onto.push_back(rj);
                }
            }
        }
    }

    void process(const ProcessArgs &args) override
    {
        if (createState == INACTIVE && startOperating)
        {
            pushMessage(std::string("Starting render in ") +
                        (testMode ? "Test Mode" : "Record Mode"));
            startOperating = false;
            createState = NEW_NOTE;
            currentJobIndex = -1;

            if (currentSampleDir.empty())
                currentSampleDir = fs::path{rack::asset::userDir} / "SampleCreator" / "Default";
            if (!testMode)
            {
                fs::create_directories(currentSampleDir);
                pushMessage("Writing to '" + currentSampleDir.u8string() + "'");
            }

            populateRenderJobs(renderJobs);
            renderThreadCommands.push(RenderThreadCommand{RenderThreadCommand::START_RENDER});
            pushMessage(std::string("Generated render jobs: " + std::to_string(renderJobs.size()) +
                                    " renders"));
            clearVU();
        }

        if (createState == INACTIVE)
        {
            outputs[OUTPUT_VOCT].setVoltage(0.f);
            outputs[OUTPUT_GATE].setVoltage(0.f);
            return;
        }

        if (createState == NEW_NOTE)
        {
            currentJobIndex++;
            playbackPos = 0;
            gateSamples = std::ceil(args.sampleRate * getParam(GATE_TIME).getValue());
            createState = GATED_RECORD;

            renderThreadCommands.push(RenderThreadCommand{
                RenderThreadCommand::NEW_NOTE, currentJobIndex, (int64_t)args.sampleRate});
            ioWriteBlock = (ioWriteBlock + 1) & (ioSampleBlocksAvailable - 1);
            ioWritePosition = 0;
        }

        auto &currentJob = renderJobs[currentJobIndex];
        outputs[OUTPUT_VOCT].setVoltage(
            std::clamp((float)currentJob.midiNote / 12.f - 5.f, -5.f, 5.f));
        outputs[OUTPUT_GATE].setVoltage((createState == GATED_RECORD) * 10.f);
        outputs[OUTPUT_VELOCITY].setVoltage(
            std::clamp((float)currentJob.velocity / 12.7f, 0.f, 10.f));
        outputs[OUTPUT_RR_UNI].setVoltage(currentJob.rrRand[0]);
        outputs[OUTPUT_RR_BI].setVoltage(currentJob.rrRand[1]);

        if (stopImmediately)
        {
            pushMessage("Stopping operation");
            clearVU();

            if (!testMode && (createState == GATED_RECORD || createState == RELEASE_RECORD))
            {
                renderThreadCommands.push(RenderThreadCommand{RenderThreadCommand::CLOSE_FILE, 0});
            }
            createState = INACTIVE;
            stopImmediately = false;
            return;
        }

        if (playbackPos > gateSamples && createState == GATED_RECORD)
        {
            createState = RELEASE_RECORD;
            memset(&silenceDetector[0], 0, sizeof(silenceDetector));
            silencePosition = 0;

            playbackPos = 0;
        }

        if (createState == RELEASE_RECORD)
        {
            float d[2];
            d[0] = inputs[INPUT_L].getVoltage() / 5.f;
            d[1] = inputs[INPUT_R].getVoltage() / 5.f;

            silenceDetector[silencePosition] = std::fabs(d[0]) + std::fabs(d[1]);
            silencePosition++;
            if (silencePosition == silenceSamples)
            {
                silencePosition = 0;

                bool silent{true};
                int32_t spos{0};

                while (silent && spos < silenceSamples)
                {
                    silent = silenceDetector[spos] < 1e-8;
                    spos++;
                }

                if (silent)
                {
                    pushMessage("Note Render complete.");
                    createState = SPINDOWN_BUFFER;
                    playbackPos = 0;
                    if (!testMode)
                    {
                        renderThreadCommands.push(
                            RenderThreadCommand{RenderThreadCommand::CLOSE_FILE, 0});
                    }
                    clearVU();
                }
            }
        }

        if (createState != SPINDOWN_BUFFER)
        {
            auto &f2 = ioBlocks[ioWriteBlock][ioWritePosition];
            f2[0] = inputs[INPUT_L].getVoltage() / 5.f;
            f2[1] = inputs[INPUT_R].getVoltage() / 5.f;

            ioWritePosition = (ioWritePosition + 1) & (ioSampleBlockSize - 1);
            // Insanely inefficient. We need to block this into a better chunk and send a chunk
            // index And when we do that we can shrink the queue again
            if (ioWritePosition == 0)
            {
                renderThreadCommands.push(
                    RenderThreadCommand{RenderThreadCommand::PUSH_SAMPLES, ioWriteBlock});
                ioWriteBlock = (ioWriteBlock + 1) & (ioSampleBlocksAvailable - 1);
            }
        }

        if (playbackPos > 1000 && createState == SPINDOWN_BUFFER)
        {
            if ((size_t)currentJobIndex == renderJobs.size() - 1)
            {
                createState = INACTIVE;
                renderThreadCommands.push(RenderThreadCommand{RenderThreadCommand::END_RENDER});

                pushMessage("Render Complete");
                clearVU();
            }
            else
            {
                createState = NEW_NOTE;
            }
        }

        playbackPos++;
    }

    void clearVU()
    {
        vuLevels[0] = 0.f;
        vuLevels[1] = 0.f;
    }

    void updateVU(int64_t whichBlock)
    {
        auto &data = ioBlocks[whichBlock];
        float vul[2];
        vul[0] = vuLevels[0] * 0.9995;
        vul[1] = vuLevels[1] * 0.9995;
        for (int i = 0; i < ioSampleBlockSize; ++i)
        {
            auto fl = fabs(data[i][0]);
            if (fl > vul[0])
                vul[0] = fl;
            auto fr = fabs(data[i][1]);
            if (fr > vul[1])
                vul[1] = fr;
        }
        vuLevels[0] = vul[0];
        vuLevels[1] = vul[1];
    }
};

} // namespace baconpaul::samplecreator
#endif // SAMPLECREATOR_SAMPLECREATORMODULE_HPP
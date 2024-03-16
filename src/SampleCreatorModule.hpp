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
static std::string midiNoteToName(int midiNote)
{
    std::vector<std::string> notes{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    auto v = midiNote;
    auto oct = v / 12 - 1;
    auto nt = v % 12;

    char res[256];
    snprintf(res, 256, "%s%d (%d)", notes[nt].c_str(), oct, v);
    return std::string(res);
}
struct MidiNoteParamQuantity : rack::ParamQuantity
{
    std::string getDisplayValueString() override
    {
        return midiNoteToName((int)std::round(getValue()));
    }
    void setDisplayValueString(std::string s) override
    {
        if ((s[0] >= 'A' && s[0] <= 'G') || (s[0] >= 'a' && s[0] <= 'g'))
        {
            int npos = 1;
            float diff{0};
            while (s[npos] == '#')
            {
                diff++;
                npos++;
            }
            while (s[npos] == 'b')
            {
                diff--;
                npos++;
            }
            auto oct = std::atoi(s.c_str() + npos);
            std::map<char, int> n2t{{'C', 0}, {'D', 2}, {'E', 4}, {'F', 5},
                                    {'G', 7}, {'A', 9}, {'B', 11}};
            auto base = n2t.at(std::toupper(s[0]));

            auto res = base + (oct + 1) * 12 + diff;
            setImmediateValue(res);
        }
        else
        {
            rack::ParamQuantity::setDisplayValueString(s);
        }
    }
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
        REL_MODE,

        RR1_TYPE,
        RR2_TYPE,

        LATENCY_COMPENSATION,
        POLYPHONY,

        VELOCITY_STRATEGY,

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

        OUTPUT_RR_ONE,
        OUTPUT_RR_TWO,

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
        configOutput(OUTPUT_RR_ONE, "RR CV One");
        configOutput(OUTPUT_RR_TWO, "RR CV Two");

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
        configSwitch(REL_MODE, 0, 1, 0, "Release Mode", {"Silence", "Drone"});

        configParam(LATENCY_COMPENSATION, 0, 512, 0, "Latency Compesnation", "Samples");
        configParam(POLYPHONY, 1, 16, 1, "Polyphony", "Voices");
        configSwitch(RR1_TYPE, 0, 2, 0, "RR1 Mode", {"0-10v", "+/-5v", "Index"});
        configSwitch(RR2_TYPE, 0, 2, 1, "RR1 Mode", {"0-10v", "+/-5v", "Index"});
        configSwitch(VELOCITY_STRATEGY, 0, 2, 1, "Velocity Strategy",
                     {"Uniform", "Sqrt", "Square"});

        renderThread = std::make_unique<std::thread>([this]() { renderThreadProcess(); });

        pushMessage("Sample Creator Started");
        pushIdle();
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
    struct MessageEntry
    {
        MessageEntry() = default;
        MessageEntry(const std::string &s) : message(s) { timeStamp = rack::system::getUnixTime(); }
        std::string message{};
        double timeStamp{0};
    };
    std::array<MessageEntry, 1024> messageBuffer;
    std::atomic<int32_t> mbWrite{0}, mbRead{0};
    void pushMessage(const std::string &s)
    {
        messageBuffer[mbWrite] = s;
        mbWrite = (mbWrite + 1) & 1023;
    }
    bool hasMessage() { return mbRead != mbWrite; }
    MessageEntry popMessage()
    {
        auto res = messageBuffer[mbRead];
        mbRead = (mbRead + 1) & 1023;
        return res;
    }

    std::array<std::array<std::string, 32>, 2> statusBuffer;
    std::array<std::atomic<int32_t>, 2> statusWrite{0, 0}, statusRead{0, 0};
    void pushStatus(const std::string &s, int buffer)
    {
        statusBuffer[buffer][statusWrite[buffer]] = s;
        statusWrite[buffer] = (statusWrite[buffer] + 1) & 31;
    }
    bool hasStatus(int buffer) { return statusWrite[buffer] != statusRead[buffer]; }
    std::string popStatus(int buffer)
    {
        auto res = statusBuffer[buffer][statusRead[buffer]];
        statusRead[buffer] = (statusRead[buffer] + 1) & 31;
        return res;
    }
    void pushIdle()
    {
        pushStatus("Idle", 0);
        pushStatus("-", 1);
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

    fs::path currentSampleDir{}, currentSampleWavDir{};

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
    std::atomic<int64_t> currentJobIndex{-1};

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
                            auto bn = currentSampleDir.filename().replace_extension();
                            auto fn = (currentSampleDir / bn.u8string()).replace_extension("sfz");
                            pushMessage("Creating '" + fn.u8string() + "'");
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
        pushMessage(std::string("Starting note ") + midiNoteToName(currentJob.midiNote) +
                    " vel=" + std::to_string(currentJob.velocity) +
                    " rr=" + std::to_string(currentJob.roundRobinIndex));

        auto bn = std::string("sample") + "_note_" + std::to_string((int)currentJob.midiNote) +
                  "_vel_" + std::to_string((int)currentJob.velocity) + "_rr_" +
                  std::to_string((int)currentJob.roundRobinIndex) + ".wav";
        auto fn = currentSampleWavDir / bn;
        if (!testMode)
        {
            int nChannels = inputs[INPUT_R].isConnected() ? 2 : 1;
            pushMessage("Writing '" + fn.filename().u8string() + "'");
            pushMessage(std::string("   - 32 bit ") + (nChannels == 2 ? "stereo" : "mono") + " @ " +
                        std::to_string(sr) + " sr");
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

            sfzFile << "<region>seq_position=" << (currentJob.roundRobinIndex + 1) << " sample=wav/"
                    << fn.filename().u8string().c_str() << " lokey=" << currentJob.noteFrom
                    << " hikey=" << currentJob.noteTo << " pitch_keycenter=" << currentJob.midiNote
                    << " lovel=" << currentJob.velFrom << " hivel=" << currentJob.velTo << "\n"
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

        auto velStrategy = (int)std::round(getParam(VELOCITY_STRATEGY).getValue());

        if (midiStart > midiEnd)
            std::swap(midiStart, midiEnd);

        auto numSteps = (int)std::ceil(1.f * (midiEnd - midiStart) / midiStep);
        auto coverDiff = numSteps * midiStep - (midiEnd - midiStart);

        onto.clear();
        for (int i = 0; i < numSteps; ++i)
        {
            auto mn = i * midiStep + midiHalf + midiStart - coverDiff / 2;
            auto nf = mn - midiHalf;
            auto nt = nf + midiStep;

            nf = std::clamp(nf, midiStart, midiEnd);
            nt = std::clamp(nt, midiStart, midiEnd);
            mn = std::clamp((nf + nt) / 2, midiStart, midiEnd);

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

                std::function<double(double)> fn = [](double x) { return x; };
                if (velStrategy == 1)
                    fn = [](double x) { return sqrt(x); };
                if (velStrategy == 2)
                    fn = [](double x) { return x * x; };

                auto mv = (int)std::round(std::clamp(fn(bv), 0., 1.) * 127);
                auto msv = (int)std::round(std::clamp(fn(sv), 0., 1.) * 127);
                auto mev = (int)std::round(std::clamp(fn(ev), 0., 1.) * 127);

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
            pushStatus(testMode ? "Test" : "Record", 0);
            pushStatus("Start", 1);
            pushMessage(std::string("Starting render in ") +
                        (testMode ? "Test Mode" : "Record Mode"));
            startOperating = false;
            createState = NEW_NOTE;
            currentJobIndex = -1;

            if (currentSampleDir.empty())
                currentSampleDir = fs::path{rack::asset::userDir} / "SampleCreator" / "Default";
            currentSampleWavDir = currentSampleDir / "wav";
            if (!testMode)
            {
                fs::create_directories(currentSampleDir);
                fs::create_directories(currentSampleWavDir);
                pushMessage("Output to '" + currentSampleDir.u8string() + "'");
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
            clearVU();
            return;
        }

        if (createState == NEW_NOTE)
        {
            currentJobIndex++;
            auto jbn = renderJobs[currentJobIndex].midiNote;
            pushStatus(std::to_string(currentJobIndex) + "/" + std::to_string(renderJobs.size()) +
                           " " + midiNoteToName(jbn),
                       1);
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
        outputs[OUTPUT_RR_ONE].setVoltage(currentJob.rrRand[0]);
        outputs[OUTPUT_RR_TWO].setVoltage(currentJob.rrRand[1]);

        if (stopImmediately)
        {
            pushMessage("Stopping operation");
            clearVU();

            if (!testMode && (createState == GATED_RECORD || createState == RELEASE_RECORD))
            {
                renderThreadCommands.push(RenderThreadCommand{RenderThreadCommand::CLOSE_FILE, 0});
            }
            createState = INACTIVE;
            currentJobIndex = -1;
            clearVU();
            pushIdle();
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
                currentJobIndex = -1;

                clearVU();
                pushIdle();
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

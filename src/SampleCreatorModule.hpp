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

#include "RIFFWavWriter.hpp"
#include "ZIPFileWriter.hpp"

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

        OUTPUT_FORMAT,

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

    enum MultiFormats
    {
        JUST_WAV,
        SFZ,
        MULTISAMPLE,
        DECENT // few places below we assume DECENT is end, configParam and setting in startRender
    } multiFormat{SFZ};

    enum ReleaseMode
    {
        SILENCE,
        LOOP_005,
        LOOP_01,
        LOOP_025,
        GATEONLY // similarly this is the end of list
    } releaseMode{SILENCE};

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

        configParam(GATE_TIME, 0.01, 16, 1, "Gate Time", "s");
        configSwitch(REL_MODE, SILENCE, GATEONLY, SILENCE, "Release Mode",
                     {"Silence", "Loop .05xf", "Loop .1xf", "Loop .25xf", "Gate Only"});

        configParam(LATENCY_COMPENSATION, 0, 512, 0, "Latency Compesnation", "Samples");
        configParam(POLYPHONY, 1, 16, 1, "Polyphony", "Voices");
        configSwitch(RR1_TYPE, 0, 2, 0, "RR1 Mode", {"0-10v", "+/-5v", "Index"});
        configSwitch(RR2_TYPE, 0, 2, 1, "RR1 Mode", {"0-10v", "+/-5v", "Index"});
        configSwitch(VELOCITY_STRATEGY, 0, 2, 1, "Velocity Strategy",
                     {"Uniform", "Sqrt", "Square"});

        configSwitch(OUTPUT_FORMAT, JUST_WAV, DECENT, SFZ, "Output Format",
                     {"Just WAV", "SFZ", "MultiSample", "Decent"});

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
        MessageEntry(const std::string &s, bool ie) : message(s), isError(ie)
        {
            timeStamp = rack::system::getUnixTime();
        }
        std::string message{};
        bool isError{false};
        double timeStamp{0};
    };
    std::array<MessageEntry, 1024> messageBuffer;
    std::atomic<int32_t> mbWrite{0}, mbRead{0};
    void pushMessage(const std::string &s)
    {
        messageBuffer[mbWrite] = s;
        mbWrite = (mbWrite + 1) & 1023;
    }
    void pushError(const std::string &s)
    {
        messageBuffer[mbWrite] = {s, true};
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
        namespace jh = sst::rackhelpers::json;
        auto res = json_object();

        json_object_set_new(res, "path", json_string(currentSampleDir.u8string().c_str()));
        return res;
    }

    void dataFromJson(json_t *rootJ) override
    {
        namespace jh = sst::rackhelpers::json;
        auto popt = jh::jsonSafeGet<std::string>(rootJ, "path");
        if (popt.has_value())
        {
            currentSampleDir = fs::path{*popt};
        }
    }

    uint64_t playbackPos{0};

    enum CreateState
    {
        INACTIVE,
        NEW_NOTE,
        GATED_RECORD,
        RELEASE_RECORD,
        GATE_RELEASE_FADE,
        SPINDOWN_BUFFER,
    } createState{INACTIVE};

    fs::path currentSampleDir{}, currentSampleWavDir{};

    riffwav::RIFFWavWriter riffWavWriter;
    std::ofstream multiFile;

    std::atomic<bool> testMode{false};
    std::atomic<bool> startOperating{false};
    std::atomic<bool> stopImmediately{false};

    std::array<std::atomic<float>, 2> vuLevels{0, 0};

    uint64_t gateSamples{0};
    uint64_t latencySamples{0};

    uint64_t gateInitValue{0};
    uint64_t latencyInitValue{0};

    static constexpr int spindownLength{1024};
    static constexpr int gateOnlyFadeLength{1024};

    static constexpr int silenceSamples{4096};
    int silencePosition;
    float silenceDetector[silenceSamples];

    struct RenderJob
    {
        int midiNote, noteFrom, noteTo;
        int velocity{90}, velFrom, velTo;
        int roundRobinIndex{0};
        int roundRobinOutOf{1};
        float rrRand[2]{0.f, 0.f};
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
            PUSH_SAMPLES,
            PUSH_SINGLE_SAMPLE,
        } message;

        int64_t data{0};
        int64_t data2{0};

        float samplL{0.f}, sampR{0.f};
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
                            sampleMultiFileStart();
                        }
                    }
                    break;
                    case RenderThreadCommand::END_RENDER:
                    {
                        pushMessage("END RENDER");
                        if (!testMode)
                        {
                            sampleMultiFileEnd();
                        }
                    }
                    break;
                    case RenderThreadCommand::NEW_NOTE:
                        renderThreadNewNote(oc->data, oc->data2);
                        break;
                    case RenderThreadCommand::CLOSE_FILE:
                        if (riffWavWriter.isOpen())
                        {
                            if (!riffWavWriter.closeFile())
                            {
                                pushMessage(riffWavWriter.errMsg);
                            }
                            sampleMultiFileAddCurrentJob(renderJobs[oc->data], riffWavWriter);
                        }
                        break;
                    case RenderThreadCommand::PUSH_SAMPLES:
                    {
                        updateVU(oc->data);
                        renderThreadWriteBlock(oc->data);
                    }
                    break;
                    case RenderThreadCommand::PUSH_SINGLE_SAMPLE:
                    {
                        if (!testMode && riffWavWriter.isOpen())
                        {
                            float f2[2]{oc->samplL, oc->sampR};
                            riffWavWriter.pushSamples(f2);
                        }
                    }
                    break;
                    default:
                        pushError("Unhandled");
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
            auto opened = riffWavWriter.openFile();
            if (!opened)
            {
                pushError(riffWavWriter.errMsg);
            }
            riffWavWriter.writeRIFFHeader();
            riffWavWriter.writeFMTChunk(sr);
            riffWavWriter.writeINSTChunk(currentJob.midiNote, currentJob.noteFrom,
                                         currentJob.noteTo, currentJob.velFrom, currentJob.velTo);
            riffWavWriter.startDataChunk();
        }
    }

    void renderThreadWriteBlock(int whichBlock)
    {
        if (testMode)
            return;
        if (!riffWavWriter.outf)
        {
            pushError("Attempted to write to unopened file");
            return;
        }
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

    /*
     * These write the multifile (SFZ, BWS, Descent, etc...)
     */
    void sampleMultiFileStart()
    {
        switch (multiFormat)
        {
        case JUST_WAV:
            pushMessage("Wav Files Only - no multi-sample format created");
            break;
        case SFZ:
        {
            auto bn = currentSampleDir.filename().replace_extension();
            auto fn = (currentSampleDir / bn.u8string()).replace_extension("sfz");
            pushMessage("MultiFile Format: SFZ");
            pushMessage("   - '" + fn.filename().u8string() + "'");
            multiFile = std::ofstream(fn);
            if (!multiFile.is_open())
            {
                pushError("Failed to open output MultiFile");
            }
            else
            {
                multiFile << "// Basic SFZ File from Rack Sample Creator\n\n";
                multiFile << "<global>\n" << std::flush;

                multiFile << "<group>";
                if (renderJobs.size() > 0 && renderJobs[0].roundRobinOutOf > 1)
                    multiFile << " seq_length=" << renderJobs[0].roundRobinOutOf;
                multiFile << "\n";
            }
        }
        break;
        case DECENT:
        {
            auto bn = currentSampleDir.filename().replace_extension();
            auto fn = (currentSampleDir / bn.u8string()).replace_extension("dspreset");
            pushMessage("MultiFile Format: Decent Sampler");
            pushMessage("   - '" + fn.filename().u8string() + "'");
            multiFile = std::ofstream(fn);
            if (!multiFile.is_open())
            {
                pushError("Failed to open output MultiFile");
            }
            else
            {
                multiFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
                multiFile << "<DecentSampler minVersion=\"1.0.0\">\n"
                          << "  <groups>\n"
                          << "    <group>\n"
                          << std::flush;
            }
        }
        break;
        case MULTISAMPLE:
        {
            auto fn = currentSampleWavDir / "multisample.xml";

            multiFile = std::ofstream(fn);
            if (!multiFile.is_open())
            {
                pushError("Failed to open output MultiFile");
            }
            else
            {
                auto nm = currentSampleDir.filename().replace_extension();
                multiFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
                multiFile << "<multisample name=\"" << nm.u8string() << "\">\n" << std::flush;
            }
        }
        break;
        }
    }

    void sampleMultiFileEnd()
    {
        switch (multiFormat)
        {
        case JUST_WAV:
            break;
        case SFZ:
        {
            if (multiFile.is_open())
            {
                pushMessage("Closing SFZ File");
                multiFile.close();
            }
        }
        break;
        case DECENT:
        {
            if (multiFile.is_open())
            {
                multiFile << "   </group>\n  </groups>\n";
                multiFile << "</DecentSampler>" << std::flush;
                pushMessage("Closing .dspreset File");
                multiFile.close();
            }
        }
        break;
        case MULTISAMPLE:
            if (multiFile.is_open())
            {
                multiFile << "</multisample>" << std::flush;
                pushMessage("Closing multisample.xml File");
                multiFile.close();

                auto zf = (currentSampleDir / currentSampleDir.filename())
                              .replace_extension(".multisample");

                pushMessage("Creating zip : " + zf.u8string());
                ziparchive::zipDirToOutputFrom(zf, currentSampleWavDir);
            }
        }
    }

    void sampleMultiFileAddCurrentJob(const RenderJob &currentJob, const riffwav::RIFFWavWriter &rw)
    {
        auto &fn = rw.outPath;
        switch (multiFormat)
        {
        case SFZ:
        {
            if (!multiFile.is_open())
                return;

            multiFile << "<region>";
            if (currentJob.roundRobinOutOf > 1)
                multiFile << " seq_position=" << (currentJob.roundRobinIndex + 1);
            multiFile << " sample=wav/" << fn.filename().u8string().c_str()
                      << " lokey=" << currentJob.noteFrom << " hikey=" << currentJob.noteTo
                      << " pitch_keycenter=" << currentJob.midiNote
                      << " lovel=" << currentJob.velFrom << " hivel=" << currentJob.velTo << "\n"
                      << std::flush;
        }
        break;
        case DECENT:
        {
            if (!multiFile.is_open())
                return;
            multiFile << "    <sample ";
            multiFile << "path=\"wav/" << fn.filename().u8string().c_str() << "\" "
                      << "loNote=\"" << currentJob.noteFrom << "\" "
                      << "hiNote=\"" << currentJob.noteTo << "\" "
                      << "rootNote=\"" << currentJob.midiNote << "\" "
                      << "loVel=\"" << currentJob.velFrom << "\" "
                      << "hiVel=\"" << currentJob.velTo << "\" " << std::flush;

            if (currentJob.roundRobinOutOf > 1)
                multiFile << " seqMode=\"round_robin\" seqLength=\"" << currentJob.roundRobinOutOf
                          << "\" "
                          << " seqPosition=\"" << currentJob.roundRobinIndex + 1 << "\" ";

            multiFile << "/>\n";
        }
        break;
        case MULTISAMPLE:
        {
            if (!multiFile.is_open())
                return;
            multiFile << "    <sample file=\"" << fn.filename().u8string() << "\" ";
            if (currentJob.roundRobinOutOf > 1)
                multiFile << " zone-logic=\"round-robin\" ";
            multiFile << " sample-start=\"0\" sample-stop=\"" << rw.getSampleCount() << "\" ";
            multiFile << ">\n";

            multiFile << "         <key "
                      << "low=\"" << currentJob.noteFrom << "\" "
                      << "high=\"" << currentJob.noteTo << "\" "
                      << "root=\"" << currentJob.midiNote << "\" "
                      << "/>\n";
            multiFile << "         <velocity "
                      << "low=\"" << currentJob.velFrom << "\" "
                      << "high=\"" << currentJob.velTo << "\"/>\n";

            multiFile << "    </sample>\n";
        }
        break;
        default:
            break;
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

        auto rrOneStrategy = (int)std::round(getParam(RR1_TYPE).getValue());
        auto rrTwoStrategy = (int)std::round(getParam(RR2_TYPE).getValue());

        if (midiStart > midiEnd)
            std::swap(midiStart, midiEnd);

        auto numSteps = (int)std::ceil(1.f * (midiEnd - midiStart + 1) / midiStep);
        auto coverDiff = numSteps * midiStep - (midiEnd - midiStart);

        onto.clear();
        for (int i = 0; i < numSteps; ++i)
        {
            auto mn = i * midiStep + midiHalf + midiStart - coverDiff / 2;
            auto nf = mn - midiHalf;
            auto nt = nf + midiStep - 1;

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

                auto mv = (int)std::round(std::clamp(fn(bv), 0., 1.) * 128);
                auto msv = (int)std::round(std::clamp(fn(sv), 0., 1.) * 128);
                auto mev = (int)std::round(std::clamp(fn(ev), 0., 1.) * 128) - 1;
                msv = std::clamp(msv, 1, 127);
                mev = std::clamp(mev, 1, 127); // we can't use 0 since thats 'off'
                mv = std::clamp(mv, msv, mev);

                auto vrj = mrj;
                vrj.velocity = mv;
                vrj.velFrom = msv;
                vrj.velTo = mev;

                for (int rr = 0; rr < numRR; ++rr)
                {
                    auto rj = vrj;
                    rj.roundRobinIndex = rr;
                    rj.roundRobinOutOf = numRR;
                    switch (rrOneStrategy)
                    {
                    case 2: // index
                        rj.rrRand[0] = (numRR == 1 ? 0 : 10.f * rr / (numRR - 1));
                        break;
                    case 1: // pm5v
                        rj.rrRand[0] = uniReal(reng) * 10.f - 5.f;
                        break;
                    default:
                    case 0: // 0-10v
                        rj.rrRand[0] = uniReal(reng) * 10.f;
                        break;
                    }
                    switch (rrTwoStrategy)
                    {
                    case 2: // index
                        rj.rrRand[1] = (numRR == 1 ? 0 : 10.f * rr / (numRR - 1));
                        break;
                    case 1: // pm5v
                        rj.rrRand[1] = uniReal(reng) * 10.f - 5.f;
                        break;
                    default:
                    case 0: // 0-10v
                        rj.rrRand[1] = uniReal(reng) * 10.f;
                        break;
                    }
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
            latencyInitValue = std::round(getParam(LATENCY_COMPENSATION).getValue());
            gateInitValue = std::ceil(args.sampleRate * getParam(GATE_TIME).getValue());
            releaseMode = (ReleaseMode)std::round(getParam(REL_MODE).getValue());

            auto iv = (int)std::round(getParam(OUTPUT_FORMAT).getValue());
            if (iv < JUST_WAV || iv > DECENT)
                iv = JUST_WAV;
            multiFormat = (MultiFormats)iv;

            if (currentSampleDir.empty())
                currentSampleDir = fs::path{rack::asset::userDir} / "SampleCreator" / "Default";
            currentSampleWavDir = currentSampleDir / "wav";

            if (multiFormat == MULTISAMPLE)
            {
                currentSampleWavDir = currentSampleDir / "raw";
            }

            if (!testMode)
            {
                try
                {
                    fs::create_directories(currentSampleDir);
                    fs::create_directories(currentSampleWavDir);
                    pushMessage("Output to '" + currentSampleDir.u8string() + "'");
                }
                catch (const fs::filesystem_error &e)
                {
                    pushError(std::string() + "Unable to create output directories : " + e.what());
                }
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
            createState = GATED_RECORD;

            latencySamples = latencyInitValue;
            gateSamples = gateInitValue;

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

            if (!testMode)
            {
                renderThreadCommands.push(
                    RenderThreadCommand{RenderThreadCommand::CLOSE_FILE, currentJobIndex});
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
            if (releaseMode == SILENCE)
            {
                createState = RELEASE_RECORD;
                memset(&silenceDetector[0], 0, sizeof(silenceDetector));
                silencePosition = 0;
            }
            else if (releaseMode == GATEONLY)
            {
                createState = GATE_RELEASE_FADE;
            }
            else
            {
                pushError("Unhandled loop mode");
                createState = SPINDOWN_BUFFER;
            }

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
                    silent = silenceDetector[spos] < 1e-6;
                    spos++;
                }

                if (silent)
                {
                    createState = SPINDOWN_BUFFER;
                    playbackPos = 0;
                    if (!testMode)
                    {
                        renderThreadCommands.push(
                            RenderThreadCommand{RenderThreadCommand::CLOSE_FILE, currentJobIndex});
                    }
                    clearVU();
                }
            }
        }

        if (createState == GATE_RELEASE_FADE)
        {
            float f2[2];
            f2[0] = inputs[INPUT_L].getVoltage() / 5.f;
            f2[1] = inputs[INPUT_R].getVoltage() / 5.f;

            auto dist = 1.0 - 1.0 * playbackPos / gateOnlyFadeLength;
            f2[0] *= dist;
            f2[1] *= dist;

            if (!testMode)
            {
                renderThreadCommands.push(RenderThreadCommand{
                    RenderThreadCommand::PUSH_SINGLE_SAMPLE, 0, 0, f2[0], f2[1]});
            }

            if (playbackPos == gateOnlyFadeLength)
            {
                if (!testMode)
                {
                    renderThreadCommands.push(
                        RenderThreadCommand{RenderThreadCommand::CLOSE_FILE, currentJobIndex});
                }
                playbackPos = 0;
                createState = SPINDOWN_BUFFER;
            }
        }
        else if (createState != SPINDOWN_BUFFER)
        {
            if (latencySamples == 0)
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
            if (latencySamples > 0)
                latencySamples--;
        }

        if (playbackPos > spindownLength * (releaseMode == GATEONLY ? 16 : 1) &&
            createState == SPINDOWN_BUFFER)
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

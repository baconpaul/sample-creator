/*
 * SampleCreator
 *
 * An experimental idea based on a preliminary convo. Probably best to come back later.
 *
 * Copyright Paul Walker 2024
 *
 * Released under the MIT License. See `LICENSE.md` for details
 */

#ifndef SRC_RIFFWAVWRITER_HPP
#define SRC_RIFFWAVWRITER_HPP

#include <ghc/filesystem.hpp>
namespace fs = ghc::filesystem;

#include "rack.hpp"

namespace baconpaul::samplecreator::riffwav
{
/*
 * A very simple RIFF Wav Writer which *just* writes F32 stereo wav files
 * with an inst block.
 */
struct RIFFWavWriter
{
    fs::path outPath{};
    FILE *outf{nullptr};
    size_t elementsWritten{0};
    size_t fileSizeLocation{0};
    size_t dataSizeLocation{0};
    size_t dataLen{0};

    uint16_t nChannels{2};

    RIFFWavWriter() {}

    RIFFWavWriter(const fs::path &p, uint16_t chan) : outPath(p), nChannels(chan) {}
    ~RIFFWavWriter() { closeFile(); }

    void writeRIFFHeader()
    {
        pushc4('R', 'I', 'F', 'F');
        fileSizeLocation = elementsWritten;
        pushi32(0);
        pushc4('W', 'A', 'V', 'E');
    }

    void writeFMTChunk(int32_t samplerate)
    {
        pushc4('f', 'm', 't', ' ');
        pushi32(16);
        pushi16(3);         // IEEE float
        pushi16(nChannels); // channels
        pushi32(samplerate);
        pushi32(samplerate * nChannels * 4); // channels * bytes * samplerate
        pushi16(nChannels * 4);              // align on pair of 4 byte samples
        pushi16(8 * 4);                      // bits per sample
    }

    void writeINSTChunk(char keyroot, char keylow, char keyhigh, char vellow, char velhigh)
    {
        pushc4('i', 'n', 's', 't');
        pushi32(8);
        pushi8(keyroot);
        pushi8(0);
        pushi8(127);
        pushi8(keylow);
        pushi8(keyhigh);
        pushi8(vellow);
        pushi8(velhigh);
        pushi8(0);
    }

    void startDataChunk()
    {
        pushc4('d', 'a', 't', 'a');
        dataSizeLocation = elementsWritten;
        pushi32(0);
    }

    void pushSamples(float d[2])
    {
        elementsWritten += fwrite(d, 1, nChannels * sizeof(float), outf);
        dataLen += 8;
    }

    void pushc4(char a, char b, char c, char d)
    {
        char f[4]{a, b, c, d};
        pushc4(f);
    }
    void pushc4(char f[4]) { elementsWritten += fwrite(f, sizeof(char), 4, outf); }

    void pushi32(int32_t i)
    {
        elementsWritten += std::fwrite(&i, sizeof(char), sizeof(uint32_t), outf);
    }

    void pushi16(int16_t i) { elementsWritten += fwrite(&i, sizeof(char), sizeof(uint16_t), outf); }

    void pushi8(char i) { elementsWritten += std::fwrite(&i, 1, 1, outf); }
    void openFile()
    {
        elementsWritten = 0;
        dataLen = 0;
        dataSizeLocation = 0;
        fileSizeLocation = 0;

        outf = fopen(outPath.u8string().c_str(), "wb");
        // TODO deal with failed open
    }
    void closeFile()
    {
        if (outf)
        {

            int res;
            res = std::fseek(outf, fileSizeLocation, SEEK_SET);
            if (res)
                std::cout << "SEEK ZERO ERROR" << std::endl;
            int32_t chunklen = elementsWritten - 8; // minus riff and size
            fwrite(&chunklen, sizeof(uint32_t), 1, outf);

            res = std::fseek(outf, dataSizeLocation, SEEK_SET);
            if (res)
                std::cout << "SEEK ONE ERROR" << std::endl;
            fwrite(&dataLen, sizeof(uint32_t), 1, outf);
            std::fclose(outf);
            outf = nullptr;
        }
    }
};
} // namespace baconpaul::samplecreator::riffwav
#endif // SAMPLECREATOR_RIFFWAVWRITER_HPP

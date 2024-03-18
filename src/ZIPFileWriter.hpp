/*
 * SampleCreator
 *
 * An experimental idea based on a preliminary convo. Probably best to come back later.
 *
 * Copyright Paul Walker 2024
 *
 * Released under the MIT License. See `LICENSE.md` for details
 */

#ifndef SRC_ZIPFILEWRITER_HPP
#define SRC_ZIPFILEWRITER_HPP

#include <archive.h>
#include <archive_entry.h>

namespace baconpaul::samplecreator::ziparchive
{
// Non-recursively travers indir to outdir
inline bool zipDirToOutputFrom(const fs::path &outFile, const fs::path &inDir)
{
    struct archive *a;
    struct archive_entry *entry;
    char buff[8192];
    size_t size;
    FILE *fp;

    a = archive_write_new();
    archive_write_set_format_zip(a);
    archive_write_open_filename(a, outFile.u8string().c_str());

    try
    {
        assert(fs::is_directory(inDir));
        for (const auto &fsentry : fs::directory_iterator(inDir))
        {
            assert(fs::is_regular_file(fsentry.path()));
            auto fs = fs::file_size(fsentry.path());

            fp = fopen(fsentry.path().u8string().c_str(), "rb");
            if (fp == NULL)
            {
                return false;
            }

            entry = archive_entry_new(); // Note 2
            archive_entry_set_pathname(entry, fsentry.path().filename().u8string().c_str());
            archive_entry_set_size(entry, fs); // Note 3
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_perm(entry, 0644);
            archive_write_header(a, entry);

            while ((size = fread(buff, 1, sizeof(buff), fp)) > 0)
            {
                archive_write_data(a, buff, size);
            }

            fclose(fp);

            archive_entry_free(entry);
        }
        archive_write_close(a); // Note 4
        archive_write_free(a);  // Note 5
        return true;
    }
    catch (const fs::filesystem_error &e)
    {
    }
    return false;
}
} // namespace baconpaul::samplecreator::ziparchive
#endif // SAMPLECREATOR_ZIPFILEWRITER_HPP

#pragma once
#include "model.hpp"
#include <cstdio>
namespace rd {
enum class ArchiveCleanup { Kept, Deleted, Failed };
// Skipped outputs have not been verified against the ZIP; retain the recovery copy.
inline ArchiveCleanup finishArchive(const std::string &path, const std::string &category, bool deleteOriginal,
                                    bool complete, bool cancelled, unsigned written, unsigned skipped) {
    if (!deleteOriginal || !complete || cancelled || !written || skipped || lower(category) == "arcade")
        return ArchiveCleanup::Kept;
    return std::remove(path.c_str()) == 0 ? ArchiveCleanup::Deleted : ArchiveCleanup::Failed;
}
} // namespace rd

#pragma once

#include "ArchiveSearch.h"

#include <string>
#include <vector>

namespace NanaZip::Search {

struct ArchiveEntry {
    std::wstring path;
    std::uint64_t size = 0;
    bool isDir = false;
    bool encrypted = false;
};

// Resolve 7z / NanaZipC / 7zz from PATH and common install locations.
std::wstring FindSevenZipCli();

// Parse `7z l -slt` listing.
bool ListArchiveEntries(
    const std::wstring& archivePath,
    const std::wstring& password,
    std::vector<ArchiveEntry>& entries,
    std::wstring& error);

// Extract one member to memory via `7z x -so`.
bool ExtractMemberToMemory(
    const std::wstring& archivePath,
    const std::wstring& innerPath,
    const std::wstring& password,
    std::uint64_t maxSize,
    std::vector<std::uint8_t>& out,
    std::wstring& error);

// Full archive search (optional nested archives).
bool ScanArchive(
    const std::wstring& archivePath,
    const SearchOptions& opt,
    const std::wstring& password,
    const HitCallback& onHit,
    ProgressCallback onProgress = {},
    int depthLeft = -1);

} // namespace NanaZip::Search

/*
 * PROJECT:    NanaZip.Search
 * FILE:       InArchiveExtractor.h
 * PURPOSE:    In-process IInArchive extract + ScanBuffer.
 *             One Extract() call for the whole archive (solid-friendly).
 */
#pragma once

#include "ArchiveSearch.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct IInArchive;

namespace NanaZip::Search {

struct InArchiveOptions
{
    std::wstring Password;
    bool SearchFileNames = true;
    bool SearchContents  = true;
    std::uint64_t MaxContentBytes = 0;
};

struct InArchiveStats
{
    unsigned ItemsTotal     = 0;
    unsigned ItemsScanned   = 0;
    unsigned ItemsSkipped   = 0;
    unsigned NestedArchives = 0;
    HRESULT  LastError      = S_OK;
};

InArchiveStats SearchInArchive(
    IInArchive* archiveHandler,
    const std::wstring& archivePath,
    const SearchQuery& query,
    const InArchiveOptions& opt,
    std::vector<Hit>& hits,
    std::function<bool(const wchar_t* currentPath, unsigned done, unsigned total)> progress = {});

InArchiveStats SearchInArchiveFile(
    const std::wstring& archivePath,
    const SearchQuery& query,
    const InArchiveOptions& opt,
    std::vector<Hit>& hits,
    std::function<bool(const wchar_t* currentPath, unsigned done, unsigned total)> progress = {});

} // namespace NanaZip::Search

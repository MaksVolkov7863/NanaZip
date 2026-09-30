#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace NanaZip::Search {

enum class MatchMode {
    Literal,
    Hex,
    Regex,
};

enum class BinaryPolicy {
    Skip,
    Scan,
};

struct SearchOptions {
    std::wstring query;
    MatchMode mode = MatchMode::Literal;
    bool caseSensitive = false;
    bool wholeWord = false;
    bool recurse = true;
    std::wstring nameMask = L"*";
    std::wstring archiveMask = L"*";
    int nestedDepth = 2;
    bool skipEncrypted = true;
    BinaryPolicy binary = BinaryPolicy::Skip;
    int contextBytes = 64;
    std::uint64_t maxFileSize = 64ull * 1024ull * 1024ull;
    std::vector<std::wstring> encodings; // empty = utf8 + utf16le + ansi
};

struct SearchHit {
    std::wstring container;   // archive or folder
    std::wstring innerPath;
    std::uint64_t offset = 0;
    std::wstring encoding;
    std::string snippet;      // UTF-8 snippet
};

struct SearchProgress {
    std::wstring currentContainer;
    std::wstring currentItem;
    std::uint64_t filesScanned = 0;
    std::uint64_t hits = 0;
    bool cancel = false;
};

using HitCallback = std::function<void(const SearchHit&)>;
using ProgressCallback = std::function<void(SearchProgress&)>;

// Scan a filesystem file as raw bytes (also used for extracted archive members).
bool ScanBuffer(
    const void* data,
    std::size_t size,
    const SearchOptions& opt,
    const std::wstring& container,
    const std::wstring& innerPath,
    const HitCallback& onHit);

// Scan a file on disk.
bool ScanFile(
    const std::wstring& path,
    const SearchOptions& opt,
    const HitCallback& onHit,
    ProgressCallback onProgress = {});

// Recursively scan a folder (files + optional nested archives when extractor is set).
bool ScanFolder(
    const std::wstring& root,
    const SearchOptions& opt,
    const HitCallback& onHit,
    ProgressCallback onProgress = {});

bool NameMatchesMask(const std::wstring& name, const std::wstring& mask);
bool LooksLikeArchive(const std::wstring& path, const std::wstring& archiveMask);
std::vector<std::uint8_t> ParseHexQuery(const std::wstring& hex);

} // namespace NanaZip::Search

#include "ArchiveSearch.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace NanaZip::Search {
namespace {

std::wstring ToLower(std::wstring s) {
    for (auto& ch : s) {
        if (ch >= L'A' && ch <= L'Z') ch = static_cast<wchar_t>(ch - L'A' + L'a');
    }
    return s;
}

std::string WideToUtf8(const std::wstring& w) {
    std::string out;
    out.reserve(w.size());
    for (wchar_t ch : w) {
        const unsigned int cp = static_cast<unsigned int>(ch);
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

bool WildMatch(const wchar_t* text, const wchar_t* pat) {
    const wchar_t* star = nullptr;
    const wchar_t* ss = text;
    while (*text) {
        if (*pat == L'*') {
            star = pat++;
            ss = text;
            continue;
        }
        if (*pat == L'?' || *pat == *text) {
            ++text;
            ++pat;
            continue;
        }
        if (star) {
            pat = star + 1;
            text = ++ss;
            continue;
        }
        return false;
    }
    while (*pat == L'*') ++pat;
    return *pat == 0;
}

bool IsBinarySample(const std::uint8_t* p, std::size_t n) {
    std::size_t limit = n < 512 ? n : 512;
    int ctrl = 0;
    for (std::size_t i = 0; i < limit; ++i) {
        if (p[i] == 0) return true;
        if (p[i] < 0x09) ++ctrl;
    }
    return ctrl > 8;
}

std::string MakeSnippet(const std::uint8_t* p, std::size_t n, std::size_t off, int ctx) {
    std::size_t begin = off > static_cast<std::size_t>(ctx) ? off - ctx : 0;
    std::size_t end = off + ctx;
    if (end > n) end = n;
    std::string out;
    out.reserve(end - begin);
    for (std::size_t i = begin; i < end; ++i) {
        unsigned char c = p[i];
        out.push_back((c >= 32 && c < 127) ? static_cast<char>(c) : '.');
    }
    return out;
}

std::vector<std::size_t> BoyerMooreHorspool(
    const std::uint8_t* hay, std::size_t n,
    const std::uint8_t* needle, std::size_t m,
    bool caseInsensitiveAscii) {
    std::vector<std::size_t> hits;
    if (m == 0 || m > n) return hits;
    std::size_t shift[256];
    for (int i = 0; i < 256; ++i) shift[i] = m;
    auto norm = [&](unsigned char c) -> unsigned char {
        if (caseInsensitiveAscii && c >= 'A' && c <= 'Z') return static_cast<unsigned char>(c - 'A' + 'a');
        return c;
    };
    for (std::size_t i = 0; i + 1 < m; ++i) shift[norm(needle[i])] = m - 1 - i;
    std::size_t i = 0;
    while (i + m <= n) {
        std::size_t j = m;
        while (j > 0 && norm(hay[i + j - 1]) == norm(needle[j - 1])) --j;
        if (j == 0) {
            hits.push_back(i);
            i += (m > 1 ? m : 1);
        } else {
            i += shift[norm(hay[i + m - 1])];
        }
    }
    return hits;
}

bool WholeWordAt(const std::uint8_t* p, std::size_t n, std::size_t off, std::size_t m) {
    auto word = [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
    };
    if (off > 0 && word(p[off - 1])) return false;
    if (off + m < n && word(p[off + m])) return false;
    return true;
}

std::wstring FileNameOf(const std::wstring& path) {
    auto pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? path : path.substr(pos + 1);
}

} // namespace

bool NameMatchesMask(const std::wstring& name, const std::wstring& mask) {
    if (mask.empty() || mask == L"*") return true;
    std::wstring n = ToLower(name);
    std::wstring m = ToLower(mask);
    std::wstring cur;
    for (std::size_t i = 0; i <= m.size(); ++i) {
        if (i == m.size() || m[i] == L' ') {
            if (!cur.empty() && WildMatch(n.c_str(), cur.c_str())) return true;
            cur.clear();
        } else {
            cur.push_back(m[i]);
        }
    }
    return false;
}

bool LooksLikeArchive(const std::wstring& path, const std::wstring& archiveMask) {
    static const wchar_t* kExt[] = {
        L".7z", L".zip", L".rar", L".tar", L".gz", L".xz", L".bz2", L".wim",
        L".iso", L".cab", L".jar", L".apk", L".nupkg", L".vsix", L".zstd", L".zst",
        L".lz4", L".lzh", L".arj", L".dmg", L".vhd", L".vhdx", nullptr};
    std::wstring lower = ToLower(path);
    bool known = false;
    for (int i = 0; kExt[i]; ++i) {
        auto el = std::wstring(kExt[i]);
        if (lower.size() >= el.size() && lower.compare(lower.size() - el.size(), el.size(), el) == 0) {
            known = true;
            break;
        }
    }
    if (!known) return false;
    return NameMatchesMask(FileNameOf(path), archiveMask);
}

std::vector<std::uint8_t> ParseHexQuery(const std::wstring& hex) {
    std::vector<std::uint8_t> out;
    int acc = -1;
    for (wchar_t ch : hex) {
        int v = -1;
        if (ch >= L'0' && ch <= L'9') v = ch - L'0';
        else if (ch >= L'a' && ch <= L'f') v = ch - L'a' + 10;
        else if (ch >= L'A' && ch <= L'F') v = ch - L'A' + 10;
        else continue;
        if (acc < 0) acc = v;
        else {
            out.push_back(static_cast<std::uint8_t>((acc << 4) | v));
            acc = -1;
        }
    }
    return out;
}

bool ScanBuffer(
    const void* data,
    std::size_t size,
    const SearchOptions& opt,
    const std::wstring& container,
    const std::wstring& innerPath,
    const HitCallback& onHit) {
    if (!data || !onHit) return false;
    const auto* p = static_cast<const std::uint8_t*>(data);
    if (opt.maxFileSize && size > opt.maxFileSize) return true;
    if (opt.mode != MatchMode::Hex && opt.binary == BinaryPolicy::Skip && IsBinarySample(p, size)) {
        return true;
    }

    auto emit = [&](std::size_t off, const wchar_t* enc) {
        SearchHit hit;
        hit.container = container;
        hit.innerPath = innerPath;
        hit.offset = off;
        hit.encoding = enc;
        hit.snippet = MakeSnippet(p, size, off, opt.contextBytes);
        onHit(hit);
    };

    if (opt.mode == MatchMode::Hex) {
        auto needle = ParseHexQuery(opt.query);
        if (needle.empty()) return false;
        auto hits = BoyerMooreHorspool(p, size, needle.data(), needle.size(), false);
        for (auto off : hits) emit(off, L"hex");
        return true;
    }

    if (opt.mode == MatchMode::Regex) {
        try {
            const std::string q = WideToUtf8(opt.query);
            auto flags = std::regex::ECMAScript;
            if (!opt.caseSensitive) flags |= std::regex::icase;
            std::regex re(q, flags);
            std::string hay(reinterpret_cast<const char*>(p), reinterpret_cast<const char*>(p) + size);
            if (hay.size() > opt.maxFileSize) return true;
            for (std::sregex_iterator it(hay.begin(), hay.end(), re), end; it != end; ++it) {
                emit(static_cast<std::size_t>(it->position()), L"regex");
            }
        } catch (...) {
            return false;
        }
        return true;
    }

    const std::string q8 = WideToUtf8(opt.query);
    auto hits8 = BoyerMooreHorspool(
        p, size,
        reinterpret_cast<const std::uint8_t*>(q8.data()), q8.size(),
        !opt.caseSensitive);
    for (auto off : hits8) {
        if (opt.wholeWord && !WholeWordAt(p, size, off, q8.size())) continue;
        emit(off, L"utf-8/ansi");
    }

    std::vector<std::uint8_t> q16;
    q16.reserve(opt.query.size() * 2);
    for (wchar_t ch : opt.query) {
        q16.push_back(static_cast<std::uint8_t>(ch & 0xFF));
        q16.push_back(static_cast<std::uint8_t>((ch >> 8) & 0xFF));
    }
    if (!q16.empty()) {
        auto hits16 = BoyerMooreHorspool(p, size, q16.data(), q16.size(), false);
        for (auto off : hits16) emit(off, L"utf-16le");
    }
    return true;
}

bool ScanFile(
    const std::wstring& path,
    const SearchOptions& opt,
    const HitCallback& onHit,
    ProgressCallback onProgress) {
    if (!NameMatchesMask(FileNameOf(path), opt.nameMask)) return true;
#ifdef _WIN32
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(h, &sz)) {
        CloseHandle(h);
        return false;
    }
    if (opt.maxFileSize && static_cast<std::uint64_t>(sz.QuadPart) > opt.maxFileSize) {
        CloseHandle(h);
        return true;
    }
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(sz.QuadPart));
    DWORD rd = 0;
    if (sz.QuadPart && !ReadFile(h, buf.data(), static_cast<DWORD>(buf.size()), &rd, nullptr)) {
        CloseHandle(h);
        return false;
    }
    CloseHandle(h);
    if (onProgress) {
        SearchProgress pr;
        pr.currentContainer = path;
        pr.currentItem = FileNameOf(path);
        onProgress(pr);
        if (pr.cancel) return true;
    }
    return ScanBuffer(buf.data(), rd, opt, path, FileNameOf(path), onHit);
#else
    std::ifstream in(WideToUtf8(path), std::ios::binary);
    if (!in) return false;
    std::vector<std::uint8_t> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return ScanBuffer(buf.data(), buf.size(), opt, path, FileNameOf(path), onHit);
#endif
}

#ifdef _WIN32
static void WalkDir(const std::wstring& root, const SearchOptions& opt,
                    const HitCallback& onHit, ProgressCallback onProgress, bool& ok) {
    WIN32_FIND_DATAW fd{};
    std::wstring spec = root + L"\\*";
    HANDLE h = FindFirstFileW(spec.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.cFileName[0] == L'.' &&
            (fd.cFileName[1] == 0 || (fd.cFileName[1] == L'.' && fd.cFileName[2] == 0))) {
            continue;
        }
        std::wstring full = root + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (opt.recurse) WalkDir(full, opt, onHit, onProgress, ok);
        } else {
            if (!ScanFile(full, opt, onHit, onProgress)) ok = false;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
#endif

bool ScanFolder(
    const std::wstring& root,
    const SearchOptions& opt,
    const HitCallback& onHit,
    ProgressCallback onProgress) {
#ifdef _WIN32
    bool ok = true;
    WalkDir(root, opt, onHit, onProgress, ok);
    return ok;
#else
    (void)root; (void)opt; (void)onHit; (void)onProgress;
    return false;
#endif
}

} // namespace NanaZip::Search

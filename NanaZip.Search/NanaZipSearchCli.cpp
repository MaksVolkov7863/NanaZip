#include "ArchiveSearch.h"
#include "SevenZipCliExtractor.h"

#include <iostream>
#include <string>

using namespace NanaZip::Search;

static void PrintHelp() {
    std::wcout <<
        L"NanaZip search  (command i, WinRAR-like)\n"
        L"Usage: nanazip-search [i] [options] <path...> <query>\n\n"
        L"  -r              recurse folders\n"
        L"  -R              do not recurse\n"
        L"  -c              case sensitive\n"
        L"  -i              case insensitive (default)\n"
        L"  -h              hex query\n"
        L"  -x              regex query\n"
        L"  -w              whole word\n"
        L"  -n <mask>       inner file mask (default *)\n"
        L"  -t <mask>       archive type mask (default *)\n"
        L"  -nested <n>     nested archive depth (default 2)\n"
        L"  -skip-enc       skip encrypted members (default)\n"
        L"  -p <pass>       archive password\n"
        L"  -max <bytes>    skip larger files (default 64MiB)\n"
        L"  -ctx <n>        snippet width\n"
        L"  -json           JSON lines\n"
        L"  -?              help\n\n"
        L"Archives are opened through NanaZipC/7z if present.\n"
        L"Examples:\n"
        L"  nanazip-search i -r backup.7z TODO\n"
        L"  nanazip-search -n *.log -x logs.rar \"fail(ed|ure)\"\n"
        L"  nanazip-search -h dump.zip DEADBEEF\n";
}

#ifdef _WIN32
#include <windows.h>
static bool IsDir(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
int wmain(int argc, wchar_t** argv) {
#else
static bool IsDir(const std::wstring&) { return false; }
int main(int argc, char** argv) {
#endif
    SearchOptions opt;
    std::vector<std::wstring> paths;
    std::wstring password;
    bool json = false;

#ifdef _WIN32
    int i0 = 1;
    if (argc > 1 && (std::wstring(argv[1]) == L"i" || std::wstring(argv[1]) == L"I")) i0 = 2;
    for (int i = i0; i < argc; ++i) {
        std::wstring a = argv[i];
#else
    int i0 = 1;
    for (int i = i0; i < argc; ++i) {
        std::wstring a(argv[i], argv[i] + strlen(argv[i]));
#endif
        auto next = [&]() -> std::wstring {
#ifdef _WIN32
            return (i + 1 < argc) ? std::wstring(argv[++i]) : std::wstring();
#else
            if (i + 1 >= argc) return {};
            ++i; return std::wstring(argv[i], argv[i] + strlen(argv[i]));
#endif
        };
        if (a == L"-?" || a == L"--help") { PrintHelp(); return 0; }
        if (a == L"-r") { opt.recurse = true; continue; }
        if (a == L"-R") { opt.recurse = false; continue; }
        if (a == L"-c") { opt.caseSensitive = true; continue; }
        if (a == L"-i") { opt.caseSensitive = false; continue; }
        if (a == L"-h") { opt.mode = MatchMode::Hex; continue; }
        if (a == L"-x") { opt.mode = MatchMode::Regex; continue; }
        if (a == L"-w") { opt.wholeWord = true; continue; }
        if (a == L"-json") { json = true; continue; }
        if (a == L"-skip-enc") { opt.skipEncrypted = true; continue; }
        if (a == L"-n") { opt.nameMask = next(); continue; }
        if (a == L"-t") { opt.archiveMask = next(); continue; }
        if (a == L"-p") { password = next(); continue; }
        if (a == L"-nested") { opt.nestedDepth = std::stoi(next()); continue; }
        if (a == L"-max") { opt.maxFileSize = std::stoull(next()); continue; }
        if (a == L"-ctx") { opt.contextBytes = std::stoi(next()); continue; }
        if (!a.empty() && a[0] == L'-') {
            std::wcerr << L"unknown switch: " << a << L"\n";
            return 2;
        }
        paths.push_back(a);
    }
    if (paths.size() < 2) { PrintHelp(); return 2; }
    opt.query = paths.back();
    paths.pop_back();

    int hits = 0;
    auto onHit = [&](const SearchHit& h) {
        ++hits;
        if (json) {
            std::wcout << L"{\"container\":\"" << h.container
                       << L"\",\"path\":\"" << h.innerPath
                       << L"\",\"offset\":" << h.offset
                       << L",\"encoding\":\"" << h.encoding << L"\"}\n";
        } else {
            std::wcout << h.container << L" | " << h.innerPath
                       << L" @ " << h.offset << L" [" << h.encoding << L"] "
                       << std::wstring(h.snippet.begin(), h.snippet.end()) << L"\n";
        }
    };

    auto walk = [&](auto& self, const std::wstring& p) -> void {
        if (IsDir(p)) {
#ifdef _WIN32
            WIN32_FIND_DATAW fd{};
            HANDLE h = FindFirstFileW((p + L"\\*").c_str(), &fd);
            if (h == INVALID_HANDLE_VALUE) return;
            do {
                if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == 0 || fd.cFileName[1] == L'.')) continue;
                std::wstring full = p + L"\\" + fd.cFileName;
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (opt.recurse) self(self, full);
                } else {
                    self(self, full);
                }
            } while (FindNextFileW(h, &fd));
            FindClose(h);
#endif
            return;
        }
        if (LooksLikeArchive(p, opt.archiveMask)) {
            if (!ScanArchive(p, opt, password, onHit)) {
                // fallback: raw file scan
                ScanFile(p, opt, onHit);
            }
        } else {
            ScanFile(p, opt, onHit);
        }
    };

    auto exe = FindSevenZipCli();
    if (exe.empty()) {
        std::wcerr << L"warning: NanaZipC/7z not in PATH; archive members will not be unpacked\n";
    } else {
        std::wcerr << L"using extractor: " << exe << L"\n";
    }

    for (const auto& p : paths) walk(walk, p);
    std::wcerr << L"hits: " << hits << L"\n";
    return hits > 0 ? 0 : 1;
}

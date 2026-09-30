#include "ArchiveSearch.h"

#include <iostream>
#include <string>

using namespace NanaZip::Search;

static void PrintHelp() {
    std::wcout <<
        L"NanaZip archive/file search\n"
        L"Usage: nanazip-search [options] <path> [path...] <query>\n\n"
        L"  -r              recurse folders (default on)\n"
        L"  -R              do not recurse\n"
        L"  -c              case sensitive\n"
        L"  -i              case insensitive (default)\n"
        L"  -h              hex query\n"
        L"  -x              regex query\n"
        L"  -w              whole word\n"
        L"  -n <mask>       file name mask (default *)\n"
        L"  -max <bytes>    skip larger files\n"
        L"  -ctx <n>        snippet width\n"
        L"  -json           JSON lines output\n"
        L"  -?              help\n\n"
        L"Examples:\n"
        L"  nanazip-search -r -n *.txt C:\\backup TODO\n"
        L"  nanazip-search -h dump.bin DEADBEEF\n"
        L"  nanazip-search -x app.log fail(ed|ure)\n";
}

#ifdef _WIN32
static bool IsDir(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
#else
static bool IsDir(const std::wstring&) { return false; }
#endif

#ifdef _WIN32
#include <windows.h>
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    SearchOptions opt;
    std::vector<std::wstring> paths;
    bool json = false;
#ifdef _WIN32
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
#else
    for (int i = 1; i < argc; ++i) {
        std::wstring a(argv[i], argv[i] + strlen(argv[i]));
#endif
        if (a == L"-?" || a == L"--help") { PrintHelp(); return 0; }
        if (a == L"-r") { opt.recurse = true; continue; }
        if (a == L"-R") { opt.recurse = false; continue; }
        if (a == L"-c") { opt.caseSensitive = true; continue; }
        if (a == L"-i") { opt.caseSensitive = false; continue; }
        if (a == L"-h") { opt.mode = MatchMode::Hex; continue; }
        if (a == L"-x") { opt.mode = MatchMode::Regex; continue; }
        if (a == L"-w") { opt.wholeWord = true; continue; }
        if (a == L"-json") { json = true; continue; }
        if (a == L"-n" && i + 1 < argc) {
#ifdef _WIN32
            opt.nameMask = argv[++i];
#else
            ++i; opt.nameMask = std::wstring(argv[i], argv[i] + strlen(argv[i]));
#endif
            continue;
        }
        if (a == L"-max" && i + 1 < argc) {
#ifdef _WIN32
            opt.maxFileSize = std::stoull(argv[++i]);
#else
            opt.maxFileSize = std::stoull(argv[++i]);
#endif
            continue;
        }
        if (a == L"-ctx" && i + 1 < argc) {
            opt.contextBytes = std::stoi(
#ifdef _WIN32
                argv[++i]
#else
                argv[++i]
#endif
            );
            continue;
        }
        if (!a.empty() && a[0] == L'-') {
            std::wcerr << L"unknown switch: " << a << L"\n";
            return 2;
        }
        paths.push_back(a);
    }
    if (paths.size() < 2) {
        PrintHelp();
        return 2;
    }
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

    for (const auto& p : paths) {
        if (IsDir(p)) ScanFolder(p, opt, onHit);
        else ScanFile(p, opt, onHit);
    }
    std::wcerr << L"hits: " << hits << L"\n";
    return hits > 0 ? 0 : 1;
}

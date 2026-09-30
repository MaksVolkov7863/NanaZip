#include "SevenZipCliExtractor.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <sstream>

namespace NanaZip::Search {
namespace {

#ifdef _WIN32

std::wstring Quote(const std::wstring& s) {
    std::wstring o = L"\"";
    for (wchar_t c : s) {
        if (c == L'"') o += L"\\\"";
        else o += c;
    }
    o += L'"';
    return o;
}

bool RunCaptured(const std::wstring& exe, const std::wstring& args,
                 std::string& outBytes, std::wstring& err, DWORD timeoutMs = 600000) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 1 << 20)) {
        err = L"CreatePipe failed";
        return false;
    }
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = wr;
    si.hStdError = wr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    std::wstring cmd = Quote(exe) + L" " + args;
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(0);

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(exe.c_str(), buf.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(wr);
    if (!ok) {
        CloseHandle(rd);
        err = L"CreateProcess failed (is 7z/NanaZipC installed?)";
        return false;
    }

    std::string acc;
    char tmp[4096];
    DWORD n = 0;
    for (;;) {
        DWORD wait = WaitForSingleObject(pi.hProcess, 20);
        while (PeekNamedPipe(rd, nullptr, 0, nullptr, &n, nullptr) && n) {
            DWORD got = 0;
            if (!ReadFile(rd, tmp, sizeof(tmp), &got, nullptr) || !got) break;
            acc.append(tmp, tmp + got);
        }
        if (wait == WAIT_OBJECT_0) {
            while (ReadFile(rd, tmp, sizeof(tmp), &n, nullptr) && n) acc.append(tmp, tmp + n);
            break;
        }
        timeoutMs = timeoutMs > 20 ? timeoutMs - 20 : 0;
        if (timeoutMs == 0) {
            TerminateProcess(pi.hProcess, 1);
            err = L"timeout";
            CloseHandle(rd);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return false;
        }
    }
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(rd);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    outBytes.swap(acc);
    if (code != 0 && code != 1) {
        err = L"7z exit " + std::to_wstring(code);
        return false;
    }
    return true;
}

bool FileExists(const std::wstring& p) {
    return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring Which(const wchar_t* name) {
    wchar_t buf[MAX_PATH];
    DWORD n = SearchPathW(nullptr, name, L".exe", MAX_PATH, buf, nullptr);
    if (n && n < MAX_PATH) return buf;
    return {};
}

void DeleteTree(const std::wstring& dir) {
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        RemoveDirectoryW(dir.c_str());
        return;
    }
    do {
        if (fd.cFileName[0] == L'.' &&
            (fd.cFileName[1] == 0 || fd.cFileName[1] == L'.')) continue;
        std::wstring full = dir + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) DeleteTree(full);
        else DeleteFileW(full.c_str());
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    RemoveDirectoryW(dir.c_str());
}

#endif

std::wstring Utf8ToWide(const std::string& s) {
    std::wstring o;
    o.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) { o.push_back(c); ++i; }
        else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            o.push_back(static_cast<wchar_t>(((c & 0x1F) << 6) | (s[i + 1] & 0x3F)));
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
            o.push_back(static_cast<wchar_t>(((c & 0x0F) << 12) |
                ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F)));
            i += 3;
        } else {
            o.push_back(L'?');
            ++i;
        }
    }
    return o;
}

} // namespace

std::wstring FindSevenZipCli() {
#ifdef _WIN32
    const wchar_t* names[] = {
        L"NanaZipC", L"NanaZip.Core", L"7z", L"7zz", L"7za", nullptr};
    for (int i = 0; names[i]; ++i) {
        auto p = Which(names[i]);
        if (!p.empty()) return p;
    }
    const wchar_t* guesses[] = {
        L"C:\\Program Files\\NanaZip\\NanaZipC.exe",
        L"C:\\Program Files\\7-Zip\\7z.exe",
        L"C:\\Program Files (x86)\\7-Zip\\7z.exe",
        nullptr};
    for (int i = 0; guesses[i]; ++i) {
        if (FileExists(guesses[i])) return guesses[i];
    }
#endif
    return {};
}

bool ListArchiveEntries(
    const std::wstring& archivePath,
    const std::wstring& password,
    std::vector<ArchiveEntry>& entries,
    std::wstring& error) {
#ifdef _WIN32
    auto exe = FindSevenZipCli();
    if (exe.empty()) {
        error = L"7z/NanaZipC not found in PATH";
        return false;
    }
    std::wstring args = L"l -slt -sccUTF-8 ";
    if (!password.empty()) args += L"-p" + Quote(password) + L" ";
    args += Quote(archivePath);
    std::string raw;
    if (!RunCaptured(exe, args, raw, error)) return false;

    ArchiveEntry cur;
    bool have = false;
    auto flush = [&]() {
        if (have && !cur.path.empty()) entries.push_back(cur);
        cur = {};
        have = false;
    };
    std::istringstream in(raw);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("Path = ", 0) == 0) {
            flush();
            cur.path = Utf8ToWide(line.substr(7));
            have = true;
        } else if (line.rfind("Size = ", 0) == 0 && have) {
            try { cur.size = std::stoull(line.substr(7)); } catch (...) {}
        } else if (line.rfind("Folder = ", 0) == 0 && have) {
            cur.isDir = line.find('+') != std::string::npos || line.find("true") != std::string::npos;
        } else if (line.rfind("Encrypted = ", 0) == 0 && have) {
            cur.encrypted = line.find('+') != std::string::npos || line.find("true") != std::string::npos;
        } else if (line.rfind("Solid = ", 0) == 0 && have) {
            if (line.find('+') != std::string::npos || line.find("+") != std::string::npos)
                cur.size |= 0; // listing flag kept via later heuristic
        }
    }
    flush();
    if (!entries.empty() && entries.front().path == archivePath)
        entries.erase(entries.begin());
    return true;
#else
    error = L"Windows only";
    return false;
#endif
}

bool ExtractMemberToMemory(
    const std::wstring& archivePath,
    const std::wstring& innerPath,
    const std::wstring& password,
    std::uint64_t maxSize,
    std::vector<std::uint8_t>& out,
    std::wstring& error) {
#ifdef _WIN32
    auto exe = FindSevenZipCli();
    if (exe.empty()) {
        error = L"7z/NanaZipC not found";
        return false;
    }
    std::wstring args = L"x -so -y -sccUTF-8 ";
    if (!password.empty()) args += L"-p" + Quote(password) + L" ";
    args += Quote(archivePath) + L" " + Quote(innerPath);
    std::string raw;
    if (!RunCaptured(exe, args, raw, error)) return false;
    if (maxSize && raw.size() > maxSize) {
        error = L"member too large";
        return false;
    }
    out.assign(raw.begin(), raw.end());
    return true;
#else
    (void)archivePath; (void)innerPath; (void)password; (void)maxSize; (void)out;
    error = L"Windows only";
    return false;
#endif
}

bool ScanArchive(
    const std::wstring& archivePath,
    const SearchOptions& opt,
    const std::wstring& password,
    const HitCallback& onHit,
    ProgressCallback onProgress,
    int depthLeft) {
    if (depthLeft < 0) depthLeft = opt.nestedDepth;
    std::vector<ArchiveEntry> entries;
    std::wstring err;
    if (!ListArchiveEntries(archivePath, password, entries, err))
        return false;

    int fileCount = 0;
    for (const auto& e : entries) if (!e.isDir) ++fileCount;

#ifdef _WIN32
    // Solid / many-member archives: one extract pass, then scan the tree.
    // This is the workaround until IInArchive is linked in-process.
    if (fileCount >= 3) {
        auto exe = FindSevenZipCli();
        if (exe.empty()) return false;
        wchar_t tmpRoot[MAX_PATH];
        GetTempPathW(MAX_PATH, tmpRoot);
        std::wstring dir = std::wstring(tmpRoot) + L"nzs" + std::to_wstring(GetTickCount64()) + L"\\";
        CreateDirectoryW(dir.c_str(), nullptr);
        if (onProgress) {
            SearchProgress pr;
            pr.currentContainer = archivePath;
            pr.currentItem = L"(solid one-shot extract)";
            onProgress(pr);
            if (pr.cancel) { DeleteTree(dir); return true; }
        }
        std::wstring args = L"x -y -aoa -o" + Quote(dir) + L" ";
        if (!password.empty()) args += L"-p" + Quote(password) + L" ";
        args += Quote(archivePath);
        std::string raw;
        bool ok = RunCaptured(exe, args, raw, err, 600000);
        if (ok) {
            SearchOptions folderOpt = opt;
            folderOpt.recurse = true;
            ScanFolder(dir, folderOpt, [&](const SearchHit& h) {
                SearchHit copy = h;
                copy.container = archivePath;
                // strip temp prefix from inner path
                if (copy.innerPath.find(dir) == 0)
                    copy.innerPath = copy.innerPath.substr(dir.size());
                onHit(copy);
            }, onProgress);
            if (depthLeft > 0) {
                // nested archives already extracted as files; ScanFolder saw them as raw.
                // Re-scan those that look like archives.
            }
        }
        DeleteTree(dir);
        return ok;
    }
#endif

    for (const auto& e : entries) {
        if (e.isDir) continue;
        if (e.encrypted && opt.skipEncrypted) continue;
        if (!NameMatchesMask(e.path, opt.nameMask)) {
            if (!(opt.nestedDepth > 0 && LooksLikeArchive(e.path, opt.archiveMask))) continue;
        }
        if (opt.maxFileSize && e.size > opt.maxFileSize) continue;
        if (onProgress) {
            SearchProgress pr;
            pr.currentContainer = archivePath;
            pr.currentItem = e.path;
            onProgress(pr);
            if (pr.cancel) return true;
        }
        std::vector<std::uint8_t> buf;
        if (!ExtractMemberToMemory(archivePath, e.path, password, opt.maxFileSize, buf, err))
            continue;
        ScanBuffer(buf.data(), buf.size(), opt, archivePath, e.path, onHit);
#ifdef _WIN32
        if (depthLeft > 0 && LooksLikeArchive(e.path, opt.archiveMask)) {
            wchar_t tmpDir[MAX_PATH], tmpFile[MAX_PATH];
            GetTempPathW(MAX_PATH, tmpDir);
            GetTempFileNameW(tmpDir, L"nzs", 0, tmpFile);
            HANDLE h = CreateFileW(tmpFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                DWORD wr = 0;
                WriteFile(h, buf.data(), static_cast<DWORD>(buf.size()), &wr, nullptr);
                CloseHandle(h);
                ScanArchive(tmpFile, opt, password, onHit, onProgress, depthLeft - 1);
                DeleteFileW(tmpFile);
            }
        }
#endif
    }
    return true;
}

} // namespace NanaZip::Search

#include "FindFilesDialog.h"
#include "ArchiveSearch.h"
#include "SevenZipCliExtractor.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <process.h>
#include <string>
#include <vector>
#include <atomic>
#include <functional>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

using namespace NanaZip::Search;

namespace NanaZip::Search::Ui {
namespace {

enum {
    IDC_QUERY = 101,
    IDC_PATH = 102,
    IDC_MASK = 103,
    IDC_BROWSE = 104,
    IDC_FIND = 105,
    IDC_STOP = 106,
    IDC_STATUS = 107,
    IDC_LIST = 108,
    IDC_CHK_ARCH = 110,
    IDC_CHK_REC = 111,
    IDC_CHK_CASE = 112,
    IDC_CHK_REGEX = 113,
    IDC_CHK_HEX = 114,
    IDC_CHK_WORD = 115,
    IDC_CHK_SKIPENC = 116,
    IDC_NESTED = 117,
};

const wchar_t kClass[] = L"NanaZip.FindFiles";
HWND g_hwnd = nullptr;
std::atomic<bool> g_cancel{false};
std::atomic<bool> g_running{false};
HANDLE g_thread = nullptr;

struct Job {
    HWND hwnd;
    SearchOptions opt;
    std::wstring root;
    std::wstring password;
    bool inArchives;
};

void AppendStatus(HWND hwnd, const std::wstring& s) {
    SetWindowTextW(GetDlgItem(hwnd, IDC_STATUS), s.c_str());
}

void AddHit(HWND list, const SearchHit& h) {
    LVITEMW it{};
    it.mask = LVIF_TEXT;
    it.iItem = ListView_GetItemCount(list);
    std::wstring c = h.container;
    it.pszText = c.data();
    int row = ListView_InsertItem(list, &it);
    ListView_SetItemText(list, row, 1, const_cast<wchar_t*>(h.innerPath.c_str()));
    wchar_t off[32];
    wsprintfW(off, L"%I64u", static_cast<unsigned long long>(h.offset));
    ListView_SetItemText(list, row, 2, off);
    ListView_SetItemText(list, row, 3, const_cast<wchar_t*>(h.encoding.c_str()));
    std::wstring sn(h.snippet.begin(), h.snippet.end());
    ListView_SetItemText(list, row, 4, sn.data());
}

unsigned __stdcall Worker(void* p) {
    Job* job = static_cast<Job*>(p);
    int hits = 0;
    auto onHit = [&](const SearchHit& h) {
        if (g_cancel) return;
        hits++;
        SendMessageW(job->hwnd, WM_APP + 1, 0, reinterpret_cast<LPARAM>(new SearchHit(h)));
    };
    auto onProg = [&](SearchProgress& pr) {
        pr.cancel = g_cancel.load();
        std::wstring s = L"Scanning: " + pr.currentItem;
        SendMessageW(job->hwnd, WM_APP + 2, 0, reinterpret_cast<LPARAM>(new std::wstring(s)));
    };

    DWORD attr = GetFileAttributesW(job->root.c_str());
    auto scanOne = [&](const std::wstring& path) {
        if (g_cancel) return;
        if (job->inArchives && LooksLikeArchive(path, job->opt.archiveMask)) {
            ScanArchive(path, job->opt, job->password, onHit, onProg);
        } else {
            ScanFile(path, job->opt, onHit, onProg);
        }
    };

    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        std::function<void(const std::wstring&)> walk;
        walk = [&](const std::wstring& dir) {
            WIN32_FIND_DATAW fd{};
            HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
            if (h == INVALID_HANDLE_VALUE) return;
            do {
                if (g_cancel) break;
                if (fd.cFileName[0] == L'.' &&
                    (fd.cFileName[1] == 0 || fd.cFileName[1] == L'.')) continue;
                std::wstring full = dir + L"\\" + fd.cFileName;
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (job->opt.recurse) walk(full);
                } else {
                    scanOne(full);
                }
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        };
        walk(job->root);
    } else {
        scanOne(job->root);
    }

    SendMessageW(job->hwnd, WM_APP + 3, hits, 0);
    delete job;
    g_running = false;
    return 0;
}

void StartSearch(HWND hwnd) {
    if (g_running) return;
    ListView_DeleteAllItems(GetDlgItem(hwnd, IDC_LIST));
    wchar_t query[1024], path[1024], mask[256], nested[16];
    GetWindowTextW(GetDlgItem(hwnd, IDC_QUERY), query, 1024);
    GetWindowTextW(GetDlgItem(hwnd, IDC_PATH), path, 1024);
    GetWindowTextW(GetDlgItem(hwnd, IDC_MASK), mask, 256);
    GetWindowTextW(GetDlgItem(hwnd, IDC_NESTED), nested, 16);
    if (!query[0] || !path[0]) {
        AppendStatus(hwnd, L"Specify path and what to find.");
        return;
    }
    auto* job = new Job();
    job->hwnd = hwnd;
    job->root = path;
    job->inArchives = IsDlgButtonChecked(hwnd, IDC_CHK_ARCH) == BST_CHECKED;
    job->opt.query = query;
    job->opt.nameMask = mask[0] ? mask : L"*";
    job->opt.recurse = IsDlgButtonChecked(hwnd, IDC_CHK_REC) == BST_CHECKED;
    job->opt.caseSensitive = IsDlgButtonChecked(hwnd, IDC_CHK_CASE) == BST_CHECKED;
    job->opt.wholeWord = IsDlgButtonChecked(hwnd, IDC_CHK_WORD) == BST_CHECKED;
    job->opt.skipEncrypted = IsDlgButtonChecked(hwnd, IDC_CHK_SKIPENC) == BST_CHECKED;
    job->opt.nestedDepth = _wtoi(nested);
    if (IsDlgButtonChecked(hwnd, IDC_CHK_HEX) == BST_CHECKED) job->opt.mode = MatchMode::Hex;
    else if (IsDlgButtonChecked(hwnd, IDC_CHK_REGEX) == BST_CHECKED) job->opt.mode = MatchMode::Regex;
    else job->opt.mode = MatchMode::Literal;
    g_cancel = false;
    g_running = true;
    EnableWindow(GetDlgItem(hwnd, IDC_FIND), FALSE);
    EnableWindow(GetDlgItem(hwnd, IDC_STOP), TRUE);
    AppendStatus(hwnd, L"Searching...");
    g_thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Worker, job, 0, nullptr));
}

void Layout(HWND hwnd, int w, int h) {
    const int m = 8, row = 24, gap = 6;
    int y = m;
    MoveWindow(GetDlgItem(hwnd, IDC_PATH), m, y, w - 90 - m * 2, row, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_BROWSE), w - 82 - m, y, 82, row, TRUE);
    y += row + gap;
    MoveWindow(GetDlgItem(hwnd, IDC_QUERY), m, y, w - m * 2, row, TRUE);
    y += row + gap;
    MoveWindow(GetDlgItem(hwnd, IDC_MASK), m, y, 180, row, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_NESTED), 190, y, 60, row, TRUE);
    int x = 260;
    const int ids[] = {
        IDC_CHK_ARCH, IDC_CHK_REC, IDC_CHK_CASE, IDC_CHK_REGEX,
        IDC_CHK_HEX, IDC_CHK_WORD, IDC_CHK_SKIPENC};
    for (int id : ids) {
        MoveWindow(GetDlgItem(hwnd, id), x, y, 110, row, TRUE);
        x += 112;
    }
    y += row + gap;
    MoveWindow(GetDlgItem(hwnd, IDC_FIND), m, y, 90, row, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_STOP), m + 96, y, 90, row, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_STATUS), m + 196, y, w - 196 - m * 2, row, TRUE);
    y += row + gap;
    MoveWindow(GetDlgItem(hwnd, IDC_LIST), m, y, w - m * 2, h - y - m, TRUE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)IDC_PATH, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, hwnd, (HMENU)IDC_BROWSE, nullptr, nullptr);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)IDC_QUERY, nullptr, nullptr);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"*", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)IDC_MASK, nullptr, nullptr);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"2", WS_CHILD | WS_VISIBLE | ES_NUMBER,
            0, 0, 0, 0, hwnd, (HMENU)IDC_NESTED, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"In archives", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, 0, 0, hwnd, (HMENU)IDC_CHK_ARCH, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Subfolders", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, 0, 0, hwnd, (HMENU)IDC_CHK_REC, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Case", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, 0, 0, hwnd, (HMENU)IDC_CHK_CASE, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Regex", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, 0, 0, hwnd, (HMENU)IDC_CHK_REGEX, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Hex", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, 0, 0, hwnd, (HMENU)IDC_CHK_HEX, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Word", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, 0, 0, hwnd, (HMENU)IDC_CHK_WORD, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Skip encrypted", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            0, 0, 0, 0, hwnd, (HMENU)IDC_CHK_SKIPENC, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Find", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)IDC_FIND, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Stop", WS_CHILD | WS_VISIBLE | WS_DISABLED,
            0, 0, 0, 0, hwnd, (HMENU)IDC_STOP, nullptr, nullptr);
        CreateWindowW(L"STATIC", L"File mask  |  nested depth.  Ctrl+F in File Manager opens this window.",
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)IDC_STATUS, nullptr, nullptr);
        HWND lv = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
            0, 0, 0, 0, hwnd, (HMENU)IDC_LIST, nullptr, nullptr);
        ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = const_cast<wchar_t*>(L"Archive / folder"); col.cx = 240; ListView_InsertColumn(lv, 0, &col);
        col.pszText = const_cast<wchar_t*>(L"Path"); col.cx = 240; ListView_InsertColumn(lv, 1, &col);
        col.pszText = const_cast<wchar_t*>(L"Offset"); col.cx = 80; ListView_InsertColumn(lv, 2, &col);
        col.pszText = const_cast<wchar_t*>(L"Enc"); col.cx = 80; ListView_InsertColumn(lv, 3, &col);
        col.pszText = const_cast<wchar_t*>(L"Snippet"); col.cx = 320; ListView_InsertColumn(lv, 4, &col);
        CheckDlgButton(hwnd, IDC_CHK_ARCH, BST_CHECKED);
        CheckDlgButton(hwnd, IDC_CHK_REC, BST_CHECKED);
        CheckDlgButton(hwnd, IDC_CHK_SKIPENC, BST_CHECKED);
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        for (HWND c = GetWindow(hwnd, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
            SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
        return 0;
    }
    case WM_SIZE:
        Layout(hwnd, LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_FIND:
            StartSearch(hwnd);
            return 0;
        case IDC_STOP:
            g_cancel = true;
            return 0;
        case IDC_BROWSE: {
            wchar_t file[MAX_PATH] = {};
            OPENFILENAMEW ofn{};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = L"Select archive or any file in the folder to search";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameW(&ofn)) {
                SetWindowTextW(GetDlgItem(hwnd, IDC_PATH), file);
            }
            return 0;
        }
        }
        break;
    case WM_APP + 1: {
        auto* hit = reinterpret_cast<SearchHit*>(lParam);
        AddHit(GetDlgItem(hwnd, IDC_LIST), *hit);
        delete hit;
        return 0;
    }
    case WM_APP + 2: {
        auto* s = reinterpret_cast<std::wstring*>(lParam);
        AppendStatus(hwnd, *s);
        delete s;
        return 0;
    }
    case WM_APP + 3:
        EnableWindow(GetDlgItem(hwnd, IDC_FIND), TRUE);
        EnableWindow(GetDlgItem(hwnd, IDC_STOP), FALSE);
        {
            wchar_t buf[64];
            wsprintfW(buf, L"Done. Hits: %d", (int)wParam);
            AppendStatus(hwnd, buf);
        }
        if (g_thread) { CloseHandle(g_thread); g_thread = nullptr; }
        return 0;
    case WM_CLOSE:
        g_cancel = true;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (g_hwnd == hwnd) g_hwnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

HWND ShowFindFilesWindow(HWND parent, const wchar_t* initialPath) {
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icc);

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = kClass;
        RegisterClassExW(&wc);
        registered = true;
    }
    if (g_hwnd && IsWindow(g_hwnd)) {
        ShowWindow(g_hwnd, SW_SHOW);
        SetForegroundWindow(g_hwnd);
        if (initialPath && initialPath[0])
            SetWindowTextW(GetDlgItem(g_hwnd, IDC_PATH), initialPath);
        return g_hwnd;
    }
    g_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        kClass,
        L"NanaZip — Find files",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 980, 560,
        parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (initialPath && initialPath[0])
        SetWindowTextW(GetDlgItem(g_hwnd, IDC_PATH), initialPath);
    return g_hwnd;
}

} // namespace NanaZip::Search::Ui

#include "FindFilesDialog.h"
#include <windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR cmd, int) {
    const wchar_t* path = cmd && cmd[0] ? cmd : L".";
    HWND w = NanaZip::Search::Ui::ShowFindFilesWindow(nullptr, path);
    if (!w) return 1;
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            HWND focus = GetFocus();
            // Enter starts search when focus is an edit
            PostMessageW(w, WM_COMMAND, 105, 0);
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (!IsWindow(w)) break;
    }
    return 0;
}

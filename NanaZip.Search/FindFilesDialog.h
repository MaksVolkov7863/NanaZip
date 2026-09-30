#pragma once
#include <Windows.h>

namespace NanaZip::Search::Ui {

// Create (or focus) the Find files window. Thread-safe to call from FM.
HWND ShowFindFilesWindow(HWND parent, const wchar_t* initialPath);

} // namespace

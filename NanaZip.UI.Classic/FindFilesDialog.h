/*
 * PROJECT:    NanaZip
 * FILE:       FindFilesDialog.h
 * PURPOSE:    WinRAR-style Find files dialog for Classic File Manager
 */

#ifndef NANAZIP_FILEMANAGER_FINDFILESDIALOG
#define NANAZIP_FILEMANAGER_FINDFILESDIALOG

#include <Windows.h>

namespace NanaZip::FileManager::FindFilesDialog
{
    // Opens the modeless Find files window.
    // initialPath: current folder or opened archive in the File Manager.
    void Show(
        _In_opt_ HWND ParentWindowHandle,
        _In_opt_ LPCWSTR InitialPath);
}

#endif

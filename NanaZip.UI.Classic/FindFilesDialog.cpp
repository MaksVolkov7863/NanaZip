/*
 * PROJECT:    NanaZip
 * FILE:       FindFilesDialog.cpp
 * PURPOSE:    Thin Classic FM wrapper around NanaZip.Search Find window
 */

#include "FindFilesDialog.h"
#include "../NanaZip.Search/FindFilesDialog.h"

void NanaZip::FileManager::FindFilesDialog::Show(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR InitialPath)
{
    NanaZip::Search::Ui::ShowFindFilesWindow(ParentWindowHandle, InitialPath);
}

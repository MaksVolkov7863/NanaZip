/*
 * Classic File Manager command: Find files
 */

#include "FindFilesDialog.h"
#include "SevenZip/CPP/7zip/UI/FileManager/App.h"

void CApp::FindFiles()
{
    const UString& prefix = GetFocusedPanel()._currentFolderPrefix;
    NanaZip::FileManager::FindFilesDialog::Show(
        _window,
        prefix.IsEmpty() ? L"." : prefix.Ptr());
}

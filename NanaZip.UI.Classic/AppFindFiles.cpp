/* Hook used by Classic File Manager. */
#include "FindFilesDialog.h"
#include "SevenZip/CPP/7zip/UI/FileManager/App.h"
#include "SevenZip/CPP/7zip/UI/FileManager/resource.h"

#ifndef IDM_FIND_FILES
#define IDM_FIND_FILES 962
#endif

void CApp::FindFiles()
{
    const UString& prefix = GetFocusedPanel()._currentFolderPrefix;
    NanaZip::FileManager::FindFilesDialog::Show(
        _window,
        prefix.IsEmpty() ? L"." : prefix.Ptr());
}

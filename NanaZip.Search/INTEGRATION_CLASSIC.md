# Classic File Manager — Find files

## Already in the tree

- `IDM_FIND_FILES` 962 in `resource.h`
- `CApp::FindFiles()` in `App.h` + `AppFindFiles.cpp`
- `Ctrl+F` in the file list (`PanelKey.cpp`) calls `g_App.FindFiles()`
- Window implementation: `NanaZip.Search/FindFilesDialog.cpp`

## Still add when you open the project in VS

1. `NanaZip.UI.Classic/NanaZip.vcxproj` ClCompile:

```xml
<ClCompile Include="FindFilesDialog.cpp" />
<ClCompile Include="AppFindFiles.cpp" />
<ClCompile Include="..\NanaZip.Search\FindFilesDialog.cpp" />
<ClCompile Include="..\NanaZip.Search\ArchiveSearch.cpp" />
<ClCompile Include="..\NanaZip.Search\SevenZipCliExtractor.cpp" />
```

2. `MyLoadMenu.cpp` next to About:

```cpp
#include "../../../../../FindFilesDialog.h"

case IDM_FIND_FILES:
    g_App.FindFiles();
    break;
```

3. `resource.rc` Edit and Tools menus + accelerator:

```
MENUITEM "Find files...\tCtrl+F", IDM_FIND_FILES
"F", IDM_FIND_FILES, VIRTKEY, CONTROL, NOINVERT
```

Ctrl+F in the panel works after building `AppFindFiles.cpp` + search sources even before the menu RC is edited.

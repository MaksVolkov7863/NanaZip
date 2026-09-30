# Hook Find files into NanaZip Classic File Manager

1. Add to `NanaZip.UI.Classic/NanaZip.vcxproj` ClCompile items:

```xml
<ClCompile Include="FindFilesDialog.cpp" />
<ClCompile Include="..\NanaZip.Search\FindFilesDialog.cpp" />
<ClCompile Include="..\NanaZip.Search\ArchiveSearch.cpp" />
<ClCompile Include="..\NanaZip.Search\SevenZipCliExtractor.cpp" />
```

And headers:

```xml
<ClInclude Include="FindFilesDialog.h" />
```

2. In the File Manager command handler (where Help/About is dispatched,
   typically `App.cpp` / `PanelMenu.cpp` command `kFind` / IDM_EDIT_FIND):

```cpp
#include "FindFilesDialog.h"

case IDM_EDIT_FIND: // or kFind / 'F' with Ctrl
  NanaZip::FileManager::FindFilesDialog::Show(_window, currentFolderOrArchive);
  break;
```

3. Menu string: `Find files...\tCtrl+F` next to Test/Extract.

4. Until that wiring lands, run the standalone window:

```bat
cd NanaZip.Search
cl /EHsc /std:c++17 /O2 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS ^
  FindFilesApp.cpp FindFilesDialog.cpp ArchiveSearch.cpp SevenZipCliExtractor.cpp ^
  comdlg32.lib comctl32.lib user32.lib gdi32.lib ^
  /Fe:nanazip-find.exe

nanazip-find.exe D:\backups
```

Need `NanaZipC.exe` or `7z.exe` on PATH to search inside archives.

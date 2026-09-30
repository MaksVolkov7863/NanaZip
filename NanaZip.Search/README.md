# NanaZip.Search

Search files **and archive members** (WinRAR Find files / command `I`, plus regex, nested archives, JSON, GUI).

## CLI

```bat
cl /EHsc /std:c++17 /O2 /DUNICODE /D_UNICODE NanaZipSearchCli.cpp ArchiveSearch.cpp SevenZipCliExtractor.cpp /Fe:nanazip-search.exe

nanazip-search i -r backup.7z TODO
```

## Find files window (Classic-style)

```bat
cl /EHsc /std:c++17 /O2 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS ^
  FindFilesApp.cpp FindFilesDialog.cpp ArchiveSearch.cpp SevenZipCliExtractor.cpp ^
  comdlg32.lib comctl32.lib user32.lib gdi32.lib ^
  /Fe:nanazip-find.exe

nanazip-find.exe C:\backup
```

Fields match WinRAR: path, query, name mask, in archives, subfolders, case,
plus Regex, Hex, Whole word, Skip encrypted, nested depth. Results: archive,
inner path, offset, encoding, snippet.

Classic FM hook: `NanaZip.UI.Classic/FindFilesDialog.h` — see `INTEGRATION_CLASSIC.md`.

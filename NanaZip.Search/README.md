# NanaZip.Search

Search files **and archive members** (WinRAR command `I` analogue, plus regex / nested / JSON).

## Build (Windows, MSVC)

```bat
cl /EHsc /std:c++17 /O2 /DUNICODE /D_UNICODE ^
  NanaZipSearchCli.cpp ArchiveSearch.cpp SevenZipCliExtractor.cpp ^
  /Fe:nanazip-search.exe
```

MinGW:

```bat
g++ -std=c++17 -O2 -municode NanaZipSearchCli.cpp ArchiveSearch.cpp SevenZipCliExtractor.cpp -o nanazip-search.exe
```

Need `NanaZipC.exe` or `7z.exe` on PATH (or in `C:\Program Files\NanaZip` / `7-Zip`).

## Usage

```bat
nanazip-search i -r backup.7z TODO
nanazip-search -n *.log -x logs.rar "fail(ed|ure)"
nanazip-search -h -nested 3 dump.zip DEADBEEF
nanazip-search -json -r D:\inbox secret
```

Exit code: 0 if any hit, 1 if none, 2 on bad arguments.

How it works today: `7z l -slt` to list members, `7z x -so` to stream each matching member into Boyer-Moore / regex. Nested archives are written to a temp file and scanned again up to `-nested`.

Next step: replace the CLI extractor with in-process `IInArchive` from NanaZip.Core so solid archives are scanned in one decompress pass and the File Manager dialog can call the same engine.

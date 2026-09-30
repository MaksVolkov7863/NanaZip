# NanaZip.Search

Standalone archive search engine for the NanaZip fork.

Build the CLI (Windows, MSVC, C++17):

```
cl /EHsc /std:c++17 /O2 NanaZipSearchCli.cpp ArchiveSearch.cpp /Fe:nanazip-search.exe
```

or with MinGW:

```
g++ -std=c++17 -O2 NanaZipSearchCli.cpp ArchiveSearch.cpp -o nanazip-search.exe
```

The CLI can already search **inside files and folders** (name + content +
hex + regex). Archive streaming through NanaZip.Core `IInArchive` is specified
in `Documents/ArchiveSearch.md` and hooked via `IArchiveExtractor`.

Until the COM extractor is linked, pass extracted trees or use the 7-Zip
compatible listing path described in the header comments.

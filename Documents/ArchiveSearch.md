# NanaZip Archive Search

## Status (2026-09-30)

**Not finished:** in-process `IInArchive` (true single-pass solid decompress).

**Done:**

- Content / name / hex / regex engine (`NanaZip.Search`)
- CLI (`nanazip-search i ...`)
- Win32 Find files window (`nanazip-find.exe`)
- Classic hook `CApp::FindFiles()` + `IDM_FIND_FILES` (962)
- Solid workaround: **one** extract of the whole archive to a temp dir, then scan files (not `7z x` per member)

See `NanaZip.Search/IInArchive.md`.

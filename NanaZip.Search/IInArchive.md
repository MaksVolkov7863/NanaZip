# IInArchive in-process search — not done yet

Honest status:

| Piece | Status |
|---|---|
| ScanBuffer / regex / hex / UTF-16 | done |
| CLI `nanazip-search` | done |
| Win32 Find files window | done (standalone) |
| Classic `FindFiles()` API | added |
| Menu Ctrl+F in FM | partial (ID + handler stub) |
| **IInArchive Extract → ScanBuffer in-process** | **not done** |

In-process search means linking `NanaZip.Core`, opening with `CCodecs` / `CArchiveLink`, and implementing `IArchiveExtractCallback` that writes each member into a memory stream and calls `ScanBuffer` on `SetOperationResult`. Solid blocks then decompress **once**.

Until that lands, solid archives use **one** `7z x -o%TEMP%\nzs-*` and then `ScanFolder` on the tree (see `ScanArchive`). That is already far cheaper than `7z x -so` per file.

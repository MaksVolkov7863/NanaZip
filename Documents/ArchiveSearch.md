# NanaZip Archive Search

High-quality search inside archives, designed to match WinRAR *Find files*
and go beyond it.

## What WinRAR already does

- Search by file name mask (`*.txt`, `report*.docx`).
- Search by string inside archived files (case-insensitive by default).
- Case-sensitive, hex, ANSI/Unicode/OEM character tables.
- Search on disk and/or inside archives, with recursion.
- Archive type filter (`*.rar *.zip`).
- Skip encrypted archives silently.
- Results listed in the main window; extract/open from results.

Limitations in WinRAR: no regex, no nested-archive recursion as a first-class
option, no context preview lines, no JSON output, ACE unsupported, historically
no 7z content search in some versions.

## What NanaZip.Search adds

| Capability | WinRAR | NanaZip.Search |
|---|---|---|
| Name mask (`*?` and `**`) | yes | yes |
| Content text search | yes | yes |
| Case sensitive / insensitive | yes | yes |
| Hex bytes | yes | yes |
| Multi encoding (UTF-8, UTF-16 LE/BE, ANSI, OEM, UTF-32) | partial | yes |
| Regex (ECMAScript) | no | yes |
| Whole word | no | yes |
| Context lines around hit | no | yes |
| Nested archives (zip-in-zip, 7z-in-rar, …) | limited | yes, depth limit |
| Size / date / attribute filters | limited | yes |
| Skip encrypted | yes | yes (or prompt) |
| Binary skip / include | implicit | explicit |
| Parallel scan of independent archives | no | yes |
| Streaming (no full extract to disk) | yes | yes |
| CLI + JSON results | CLI `I` | CLI + JSON |
| Cancel / progress | yes | yes |

Solid 7z/RAR remain expensive: the engine still streams, but cannot randomly
seek inside a solid block. Document this in the UI (same as dnGrep).

## CLI (target)

```
nanazip i [switches] <roots...> [--] <query>

Switches:
  -r                 recurse folders
  -i                 case insensitive (default for text)
  -c                 case sensitive
  -h                 hex query (e.g. -h F0E0AEAE)
  -x                 regex
  -w                 whole word
  -n=<mask>          file name mask (default *)
  -t=<masks>         archive type masks (default * )
  -e=<enc>           encodings: utf8,utf16le,utf16be,ansi,oem,all
  -nested=<n>        nested archive depth (default 2, 0=off)
  -skip-enc          skip encrypted items
  -bin=skip|scan     binary policy (default skip for text, scan for hex)
  -ctx=<n>           context bytes/lines around match
  -max-size=<n>      skip files larger than n bytes
  -json              machine-readable output
```

Examples:

```
nanazip i -r -n=*.txt C:\backup TODO
nanazip i -c -nested=3 backup.7z "error code"
nanazip i -h -r D:\dumps DEADBEEF
nanazip i -x -n=*.log logs.rar "fail(ed|ure)"
```

## Engine design

1. Enumerate roots (files, folders, currently opened archive).
2. For each archive matching `-t`, open via `IInArchive` (NanaZip.Core).
3. Filter entries by name, size, date, attributes, encryption.
4. Extract each matching file to an in-memory sliding window (`ISequentialOutStream`).
5. Run the selected matcher (Boyer-Moore-Horspool for literal/hex, `std::regex` for regex).
6. If the entry is itself an archive and `nested > 0`, open a nested `IInArchive` on the buffered stream.
7. Emit hits: archive path, inner path, offset, encoding, optional snippet.

Do not write extracted payload to disk except when the user previews a hit.

## UI integration (Modern + Classic)

- Command **Find files…** (`Ctrl+F` / `Ctrl+Shift+F` for content).
- Dialog fields aligned with WinRAR plus Regex, Nested depth, Encodings, Context.
- Results pane: Archive | Path | Size | Offset | Encoding | Snippet.
- Actions: go to item, extract, preview, copy path, export JSON/CSV.
- Persist last dialog state (same as WinRAR Save).

Hook points:

- Classic: `NanaZip.UI.Classic` file manager command table (next to Test/Extract).
- Modern: XAML command bar + results `ListView`.
- Core: `NanaZip.Search` static library linked by CLI and both UIs.

## Status in this commit

- Specification (this file).
- Portable engine + CLI skeleton in `NanaZip.Search/`.
- Full `IInArchive` extract callback wiring is the next implementation step
  inside `NanaZip.Core` (same pattern as `CExtractCallbackImp`).

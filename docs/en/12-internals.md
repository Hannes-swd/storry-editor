[<kbd> &nbsp; ← Overview &nbsp; </kbd>](../../README.en.md)
[<kbd> &nbsp; ← Back: 11 · Settings & controls &nbsp; </kbd>](11-settings.md)

---

# 🔧 Chapter 12 · Under the hood

> **Who is this chapter for?** For the curious who want to know what is in the files –
> and for anyone working on the source. You need none of it in order to write.

---

## 12.1 The vault in detail

```
MyStory/
├── metadata.json                     project, groups, templates, colours,
│                                     own lists, graph positions
├── Characters/
│   ├── Main/
│   │   ├── Alice.md                  one element = one .md file
│   │   └── Bob.md
│   └── NPCs/
│       └── Guard.md
├── Locations/
│   ├── Castle.md
│   └── Forest.md
├── Objects/
│   └── Sword.md
├── Actions/
│   └── actions.json                  events incl. changes, tags, attachments
├── Connections/
│   └── relationships.json            connections, types with roles, blocks
├── Manuscript/
│   ├── manuscript.md                 your text with markers
│   └── gelesen.md                    reading version (generated)
├── Timeline/
│   └── events.md                     reading overview (generated)
└── Assets/
    ├── Images/
    └── Documents/
```

| Format | For | Why |
|:--|:--|:--|
| **Markdown** | Elements, manuscript | Human readable, Obsidian compatible |
| **JSON** | Actions, connections, templates | Structured data with lists and nesting |

---

## 12.2 What an element file looks like

```markdown
# Alice

<!-- story-editor: id=el_7f3a91; group=Characters/Main -->
> Diese Datei wird vom Story Editor verwaltet.
> Bitte ausschliesslich ueber die UI bearbeiten.

## Felder
- name: Alice
- age: 27
- description: A brave knight.

## Text
Free text belonging to no field.

## Beziehungen
- married_to -> [[Bob]]

## Verknuepfte Aktionen
- Day 5, 14:00 - Alice finds the sword
```

| Section | Read back? | Written? |
|:--|:--|:--|
| `<!-- story-editor: … -->` | yes – id and group | yes |
| `## Felder` | **yes** | yes |
| `## Text` | **yes** | yes |
| `## Beziehungen` | no | yes – generated from `relationships.json` |
| `## Verknuepfte Aktionen` | no | yes – generated from `actions.json` |

The two lower sections are rebuilt on every save, so edits inside them are lost –
hence the notice inside the file itself. (Section names follow the UI language;
both spellings are always accepted when reading.)

---

## 12.3 The guard against handwork

A **file watcher** observes the vault. If something outside changes a `.md` file
(Obsidian, VS Code, a sync service), the program speaks up:

```
┌─ External change ───────────────────────────────────────┐
│  ⚠️ The file "Alice.md" was modified externally!         │
│  The change may be inconsistent with the                │
│  program's data.                                        │
│                                                         │
│         [ Reload ]     [ Merge ]     [ Cancel ]         │
└─────────────────────────────────────────────────────────┘
```

| Choice | What happens |
|:--|:--|
| **Reload** | The file wins; the in-program state is discarded |
| **Merge** | External field values are taken over, extra program fields are kept |
| **Cancel** | The internal data is written back; the external change disappears |

---

## 12.4 The time model in code

```
  Everything is one number: minutes since the start of the story.

  Day 1, 00:00            →        0
  Day 1, 14:30            →      870
  Day 5, 14:00            →     6600
  Year 2, Month 3, Day 15 →   626400

  1 day   = 1440 minutes
  1 month =   30 days
  1 year  =  360 days
```

The conversion lives in `src/core/StoryTime.cpp`; `parseStoryTime()` understands German
and English spellings, `formatStoryTime()` writes in the current UI language.

### How a value at a point in time is computed

```cpp
Project::valueAt(element, field, time)
   1. take the base value from the template resp. the element
   2. walk all actions up to `time` in chronological order
   3. apply every matching change
   4. return the result
```

That is why `@Alice.age` yields two different numbers in two places without a second
value being stored anywhere.

---

## 12.5 The source

```
src/
├── core/        data model, time logic, vault I/O (Markdown+JSON),
│                manuscript parser, Word export, file watcher, self test
├── app/         Win32/DX11 host, platform helpers (dialogs, unicode paths),
│                texture cache
└── ui/          editor state (undo, selection, save queue),
                 colour scheme, language, dialogs, all windows
```

| File | Responsible for |
|:--|:--|
| `core/Manuscript.cpp` | The only place that knows the markers (`@`, `#`, `!act:`, `**`, `<u>`, `<span>`, `%%…%%`, `---`) – reads them and writes formatting back |
| `core/StoryTime.cpp` | Reading and writing time |
| `core/Project.cpp` | Data model, `valueAt()`, groups, templates |
| `core/VaultIO.cpp` | Reading and writing the vault |
| `core/DocxExport.cpp` | Builds the `.docx` package by hand (ZIP + OOXML, no third-party library) |
| `ui/DocumentView.cpp` | The writing surface: page layout with a line cache, cursor, selection, keyboard, editing through a line model |
| `ui/ManuscriptWindow.cpp` | The writing window: ribbon, navigation pane, find, status bar |
| `ui/Ribbon.cpp` | Building blocks and drawn icons of the ribbon |
| `ui/Theme.cpp` | Colour scheme – **all** colours come from here |
| `ui/Lang.cpp` | Translation table |

### Two house rules in the code

1. **No hard-coded colours.** Everything comes from `ColorScheme` (`ui/Theme.h`) or
   from the group colours.
2. **All UI text through `TR("...")`** with the German text as the key. The table lives
   in `ui/Lang.cpp`; if an entry is missing the German text remains – so the interface
   is never broken.

---

## 12.6 The self test

```sh
StoryEditor.exe --selftest
```

Almost 200 checks, no interface, exit code `0` = all good. The report also lands in
`%APPDATA%/StoryEditor/selftest.txt`. The environment variable `STORYEDITOR_HOME` redirects
settings and report to another folder – handy for testing without touching your own
configuration.

Among the things checked:

| Area | Examples |
|:--|:--|
| **Time** | Parsing, formatting, round trip |
| **Templates** | Inheritance, defaults, fields added afterwards |
| **Changes** | Value before and after an action, relative points in time |
| **Manuscript** | Recognising markers, inserting values, reading every format and writing it back losslessly, line kinds, bookmarks, comments, search |
| **Writing surface** | Typing, formatting, deleting across formatting, lists, undo, clipboard, page layout and its speed |
| **Vault** | Save and reload, renaming, older project formats |
| **Word export** | Valid ZIP, headings, formatting, alignment, lists, no markers in the text |

---

## 12.7 Dependencies

| Library | For | How it arrives |
|:--|:--|:--|
| [Dear ImGui](https://github.com/ocornut/imgui) `v1.92.9b-docking` | The whole interface | CMake `FetchContent` |
| [nlohmann/json](https://github.com/nlohmann/json) `3.11.3` | Reading/writing JSON | CMake downloads the header |
| [stb_image](https://github.com/nothings/stb) | Images for the preview | same |

Everything else is Win32 and DirectX 11 from the operating system. The finished `.exe`
runs without an installed VC++ redistributable (static CRT).

---

## 12.8 Known limits

| | |
|:--|:--|
| 🖥️ **Windows only** | The backend is Win32 + DirectX 11. No Linux or macOS port |
| 📄 **Markdown is not rendered** | The file manager shows `.md` files as raw text |
| 🧩 **No plugin system** | Planned as phase 3 of the specification, not built |

---

## ✅ In short

- Elements are Markdown, everything structured is JSON – both readable and Git friendly
- Time is internally just a number: minutes since the start of the story
- Values at a point in time are **computed**, not stored
- `--selftest` verifies almost 200 things before you rely on anything
- Colours and texts are consistently lifted out of the code

---

[<kbd> &nbsp; ← Overview &nbsp; </kbd>](../../README.en.md)
[<kbd> &nbsp; ← Back: 11 · Settings & controls &nbsp; </kbd>](11-settings.md)

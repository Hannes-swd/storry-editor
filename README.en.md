# Story Editor

*[Deutsche Fassung: README.md](README.md)*

Writing management software for complex stories – C++17 + Dear ImGui (Win32/DirectX 11),
storing everything in an Obsidian vault (Markdown + JSON).

Implementation of the specification in `claude.md` (German).

## Building

Requirements: Visual Studio 2022 (MSVC), CMake ≥ 3.20, internet access on the first configure
(ImGui, nlohmann/json and stb_image are fetched automatically).

```sh
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
./build/bin/StoryEditor.exe
```

Command line:

| Call | Effect |
| --- | --- |
| `StoryEditor.exe` | normal GUI, reopens the last project |
| `StoryEditor.exe --selftest` | checks time logic, templates, mutations, markdown and vault round trip (43 checks), exit code 0 = everything fine |
| `StoryEditor.exe --demo <folder>` | creates a demo project (Alice, Bob, Castle, Sword, actions, connections) |

## Language

The interface ships in **German and English**. Switch it under **View → Language / Sprache** or
in **Settings → Colours**. On the very first start the Windows display language decides; after
that the choice is kept in `settings.json`.

The language also covers the generated texts: time stamps (`Day 5, 14:00` / `Tag 5, 14:00`),
durations, and the section headings inside the element `.md` files. Both spellings are always
accepted when reading, so switching the language never breaks an existing vault.

## Windows

| Window | Spec | Content |
| --- | --- | --- |
| **Manuscript** | - | Writing surface with clickable markers, outline, read mode, "action from paragraph", Word export |
| **Timeline** | 3.1 | Tracks per group/element, events as dots, non-linear time scale (dense areas stretched, empty stretches compressed and marked "~ 5 days"), hover tooltip, click, context menu, drag & drop with ghost preview, double click = new action, Ctrl+wheel = zoom, filters by group/type/element/time window, resizable track title column |
| **Group manager** | 3.2 | Hierarchy tree with colours, drag & drop, context menus, template editor with field dialog (9 field types, required fields, defaults, enum options), inheritance to subgroups |
| **Details** | 3.1.7 / 3.2.3 | Detail panel for element, action, group or connection – fields editable inline, value history over time, relations, linked actions |
| **Actions** | 3.5 | Table with sorting, filters (involved, type, tag, period, full text), expandable detail rows, live sync with the timeline |
| **Story visualizer** | 3.3 | Read-only chronicle: day headings, "… pass" gaps, attribute changes, filters by element/storyline/importance |
| **Connections** | 3.4 | Network graph, draggable nodes, edges with type label and arrow, blocks (grouped connections), focus on one element with degree 1–3, type filter |
| **File manager** | 3.6 | Structured view (groups → files) and the raw vault, upload (dialog or drag & drop from Explorer), rename/move/delete, image preview, link a file to an element |
| **Settings** | 2.3 / 7.1 | Language, theme (light/dark), every colour of the scheme, group colours, timeline parameters, management of the custom lists; stored in `%APPDATA%/StoryEditor/settings.json` |

Window layout: Blender-style docking (ImGui dockspace, multi-viewport – windows can be pulled out
of the main window). Positions and sizes live in `%APPDATA%/StoryEditor/imgui_layout_v2.ini`,
visible windows in `settings.json`.

## Theme

Two built-in colour worlds, switchable under **View → Theme** or **Settings → Colours**:

- **Light** (default): paper white surfaces, dark text, graphite accent
- **Dark**: neutral dark grey with white text

Neither has a blue cast – the accent is a grey tone in both. Interactive surfaces (buttons, tabs,
headers) are mixed from the panel and accent colour, which keeps the text readable in both
themes. The automatically assigned group colours adapt their saturation and brightness.

Every single colour can be adjusted below; **Reload theme** restores the selected preset.

## The manuscript

This is where you write - the text is the point, the structure falls out of it. Short markers in
the text are clickable variables while writing and become values when reading:

| Marker | Meaning |
| --- | --- |
| `@Alice` | reference to an element; shows the name, click opens the details |
| `@Alice.age` | value of that field **at this point in the story** (mutations applied) |
| `@Characters/Main/Alice` | full path when two elements share a name |
| `#Day 5, 14:00` | from here on this point in time applies |
| `## Chapter 1` | heading, shows up in the outline |
| `!act:…` | linked action - typed as `!` or via "Place action"; **invisible** in the finished text, purely organisational |

Typing `@` suggests matching elements (Tab accepts). **"Action from paragraph"** turns the
paragraph at the cursor into an action: title from the first sentence, time from the last time
marker, involved elements from the `@` references in that paragraph - so the timeline fills up
while you write instead of through forms.

Time and action markers are **never** part of the finished text - they only control which
values apply and where an action belongs. While reading they can be shown via "Show
markers".

**Read** shows the same text without markers: names inserted, values resolved, actions as links.
Both are saved - `Manuscript/manuscript.md` with the markers and `Manuscript/gelesen.md` as the
finished reading text for Obsidian.

### Exporting as a Word file

"As Word" in the manuscript bar - or *File -> Manuscript as Word (.docx)...* - writes exactly
that finished text as a `.docx`: values baked in, time and action markers left out. The project
name becomes the title, `## Chapter` becomes a Word heading, blank lines separate paragraphs.
No Word installation is needed to produce it - the program builds the package itself.

## Connections: your own templates with roles

The program knows no built-in notions like "person" or "place" - connections are defined by you
too. Under **Connections → "Manage types…"** you create a template:

```
Name:     person at place
Role 1:   Person  →  group Characters   (or "all groups")
Role 2:   Place   →  group Locations
          any number of further roles

[x] Can be set on the timeline (temporal)
    [x] only one at a time per element
    [x] draw a band in the timeline
    Band runs in the track of: Person    Labelled with: Place
```

Each role only offers elements from its allowed groups. **Temporal** types have a "from" and an
optional "until"; with **exclusive** types the previous setting ends automatically when a new one
starts - that is a whereabouts feature without the program ever knowing the word "place".

It shows up in three places:

- **Timeline:** a coloured band behind the track, labelled with the other end
  (`▓ Castle ▓│▓ Forest ▓│▓ Castle →`). Right click a track to set a new connection, click the
  band to select it. **"Bands"** toggles types on and off.
- **Connections graph:** each circle shows what currently applies (`person at place: Castle`).
  Types with more than two roles are drawn as a star with a hub. At the bottom sits the collapsible
  **timeline**: collapsed all connections apply, expanded only those valid on the chosen day
  (with step buttons and a "5 / 7" counter).
- **Details:** roles, period and the elements of the selected connection.

Projects from an older version keep loading: the former free-text `type` becomes a type with two
roles, `source`/`target` become those roles, and date fields are parsed.

## Custom entries instead of fixed choices

Wherever a dropdown offers a list, the defaults are only a starting point – every dropdown has a
**"Custom entry"** field at the bottom (Enter or `+`). The entry is applied immediately and
stored in `metadata.json`:

| Place | List |
| --- | --- |
| Action dialog → Type | action types (Dialogue, Action, … + your own) |
| Action dialog → Storyline | storylines |
| Action dialog → Tags | free text, `Existing…` offers tags already in use |
| Action dialog → Mutations → Field | a new field name creates the field on the element |
| Connection dialog → Type | connection types (your own templates with roles, see above) |
| Connection dialog → Block | a new name creates the block |
| Element fields of type Enum | a new option lands in the template of the defining group |
| Template editor → Field | field name, description and enum options are free anyway |

The lists are managed under **Settings → Custom lists**: each entry shows how often it is used,
renaming carries every usage along, deleting reassigns them. Values that appear in the data but
are missing from a list are added back automatically on load. Only the nine field data types
(Text, Integer, …) and the time units are fixed in code.

## Keyboard shortcuts

`Ctrl+N` new element · `Ctrl+Shift+N` new project · `Ctrl+G` new group · `Ctrl+T` new action ·
`Ctrl+S` save · `Ctrl+Z`/`Ctrl+Y` undo/redo · `Ctrl+wheel` zoom the timeline · `Shift+wheel`
scroll sideways.

## Storage (Obsidian vault)

```
MyStory/
├── metadata.json                     project, groups, templates, colours, graph positions
├── Characters/Main/Alice.md          one element = one .md file
├── Locations/Castle.md
├── Objects/Sword.md
├── Actions/actions.json              actions including mutations, tags, attachments
├── Connections/relationships.json    connections and blocks
├── Timeline/events.md                generated reading overview for Obsidian
└── Assets/Images, Assets/Documents   uploaded files
```

Element file:

```markdown
# Alice

<!-- story-editor: id=el_...; group=Characters/Main -->
> This file is managed by Story Editor. Please edit it through the UI only.

## Fields
- name: Alice
- age: 27
- description: A brave knight.

## Text
Free text

## Relations
- married_to -> [[Bob]]

## Linked actions
- Day 5, 14:00 - Alice finds the sword
```

`## Relations` and `## Linked actions` are generated from actions/relationships; only `## Fields`
and `## Text` are read back. German section names (`## Felder`, `## Beziehungen`,
`## Verknuepfte Aktionen`) are accepted as well.

**.md files are written by the program only.** A file watcher detects external changes and asks
with **Reload / Merge / Cancel** (reload = the file wins, merge = take external field values and
keep additional program fields, cancel = write the program data back).

## Time model

Time is stored as minutes since the start of the story; the fictional calendar uses 30-day months
and 12-month years. Input like `Day 5, 14:30`, `Tag 5, 14:30`, `Year 2, month 3, day 15` or
`14:30` is understood, and actions can be placed relative to another action ("120 minutes
after …").

Attribute changes (mutations) hang off actions: `Alice.age: 27 → 28`. The value of a field at a
point in time is the base value plus every mutation up to then (`Project::valueAt`), shown in the
detail panel and in the story visualizer.

## Saving & undo

Everything is saved automatically after each change (can be turned off under View → Autosave).
Undo/redo works on snapshots of the whole project (64 steps) and writes the restored state
straight into the vault.

## Source layout

```
src/core     data model, time logic, vault I/O (markdown+json), file watcher, self test
src/app      Win32/DX11 host, platform helpers (dialogs, unicode paths), texture cache
src/ui       editor state (undo, selection, save queue), theme, language, dialogs, windows
```

Colours come exclusively from `ColorScheme` (`src/ui/Theme.h`) and the group colours – no
hardcoded RGB values in the windows. UI texts go through `TR("...")` with the German string as
the key; the table lives in `src/ui/Lang.cpp`.

## Known limits

- The backend is Win32/DirectX 11 (no Linux/macOS port).
- Markdown is shown as raw text in the file manager, not rendered.
- No plugin system (phase 3 of the specification).

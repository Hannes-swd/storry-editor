[<kbd> &nbsp; ← Overview &nbsp; </kbd>](../../README.en.md)
[<kbd> &nbsp; ← Back: 10 · Files, images & export &nbsp; </kbd>](10-files.md)
[<kbd> &nbsp; Next: 12 · Under the hood → &nbsp; </kbd>](12-internals.md)

---

# ⚙️ Chapter 11 · Settings & controls

> **What is this about?** Language, appearance, your own dropdown lists, keyboard
> shortcuts – and how saving and undo work.

---

## 11.1 Language

**View → Sprache / Language** – or in the settings.

The switch takes effect immediately, without a restart, and affects more than the
buttons:

| | Deutsch | English |
|:--|:--|:--|
| Times | `Tag 5, 14:00` | `Day 5, 14:00` |
| Durations | `3 Tage` | `3 days` |
| Sections in `.md` | `## Felder` | `## Fields` |

> 🛟 **No risk:** both spellings are always accepted when **reading**. Switching the
> language therefore never breaks an existing project.

On the very first start the Windows display language decides; after that your choice
lives in `settings.json`.

---

## 11.2 Theme and colours

**View → Theme** or **Settings → Colours**

| Theme | Description |
|:--|:--|
| ☀️ **Light** (default) | Paper-white surfaces, dark type, graphite accent |
| 🌙 **Dark** | Neutral dark grey with white type |

Neither has a blue cast. Interactive surfaces are mixed from the panel and accent
colours so that type stays readable in both themes.

### Every colour individually

Below that sits the complete palette:

```
  Text primary           Timeline background
  Text secondary         Timeline grid
  Background             Timeline track (alternating)
  Panel                  Timeline ruler
  Accent                 Selection
  Warning                Hover
  Success                Ghost (drag)
  Error                  Connection line
                         Node outline
```

**Reload theme** restores the chosen preset if you have clicked yourself into a corner.

> 🎨 **No colours are hard-coded.** Everything comes from the colour scheme or from the
> group colours – which is why everything really is changeable.

### Group colours

Every group has its own colour (→ [Chapter 4](04-groups.md)). When the program assigns
them automatically it keeps enough distance between hues and adapts saturation and
brightness to the theme.

---

## 11.3 Your own entries instead of fixed choices

Everywhere a dropdown is offered, the presets are only a starting point. At the bottom
of each menu there is a field for **"own entry"**:

```
 ┌─ Type ─────────────────────┐
 │  Dialogue                  │
 │  Action                    │
 │  Scene Change              │
 │  Discovery                 │
 │  Conflict                  │
 │ ────────────────────────── │
 │  Own entry:                │
 │  [ Flashback         ] [+] │ ← type, press Enter – done
 └────────────────────────────┘
```

This applies in these places:

| Place | List |
|:--|:--|
| Action dialog → Type | Action types |
| Action dialog → Strand | Plot strands |
| Action dialog → Tags | Free text; *Existing…* offers already used ones |
| Action dialog → Changes → Field | A new field name creates the field on the element |
| Connection dialog → Type | Connection types |
| Connection dialog → Block | A new name creates the block |
| Element fields of type **Enum** | A new option lands in the group's template |

### Managing the lists

**Settings → Own lists**

```
 ┌─ Action types ─────────────────────────────────────┐
 │  Dialogue        2×   [ Rename ] [ ✕ ]             │
 │  Action          1×   [ Rename ] [ ✕ ]             │
 │  Discovery       1×   [ Rename ] [ ✕ ]             │
 │  Flashback       0×   [ Rename ] [ ✕ ]             │
 └────────────────────────────────────────────────────┘
      │                     │           │
      │                     │           └─ Deleting asks which entry
      │                     │              the usages move to
      │                     └─ Renaming carries all usages along
      └─ how often the entry is used in the project
```

Values that appear in the data but are missing from the list are added automatically on
load – so you cannot lose anything.

> 🔒 **Only two things are fixed:** the nine field data types and the time units. Those
> are anchored in the code.

---

## 11.4 Saving

| Setting | Behaviour |
|:--|:--|
| ☑ **Autosave** (default) | Written after a short pause in typing, at the latest after 5 seconds |
| ☐ off | Only on <kbd>Ctrl</kbd>+<kbd>S</kbd>, by clicking the save indicator, or when closing |

Found under **View → Autosave**.

You see the state on the right of the manuscript bar:

| | |
|:--|:--|
| 🟢 **Saved** | Everything is in the vault |
| 🟡 **[ Save ]** | There are unwritten changes – clicking writes immediately |

> ⚙️ **Why the pause?** Without it the entire manuscript file would be rewritten on
> every single keystroke. The short delay spares your disk and your nerves.

---

## 11.5 Undo

<kbd>Ctrl</kbd>+<kbd>Z</kbd> and <kbd>Ctrl</kbd>+<kbd>Y</kbd>, or via **Edit**.

- **64 steps** are kept
- Each step is a snapshot of the **whole project** – deleting, renaming and moving can
  all be taken back
- The menu says **what** will be undone: *"Undo: Action from paragraph"*
- The restored state is written to the vault immediately

---

## 11.6 Keyboard shortcuts

| Key | Effect |
|:--|:--|
| <kbd>Ctrl</kbd>+<kbd>N</kbd> | New element |
| <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>N</kbd> | New project |
| <kbd>Ctrl</kbd>+<kbd>G</kbd> | New group |
| <kbd>Ctrl</kbd>+<kbd>T</kbd> | New action |
| <kbd>Ctrl</kbd>+<kbd>O</kbd> | Open project |
| <kbd>Ctrl</kbd>+<kbd>S</kbd> | Save |
| <kbd>Ctrl</kbd>+<kbd>Z</kbd> / <kbd>Ctrl</kbd>+<kbd>Y</kbd> | Undo / redo |

**Manuscript only:**

| Key | Effect |
|:--|:--|
| <kbd>Ctrl</kbd>+<kbd>F</kbd> | Find |
| <kbd>F3</kbd> / <kbd>Shift</kbd>+<kbd>F3</kbd> | Next / previous hit |
| <kbd>Ctrl</kbd>+<kbd>B</kbd> | Bold |
| <kbd>Ctrl</kbd>+<kbd>I</kbd> | Italic |
| <kbd>@</kbd> | Suggestion list for elements |
| <kbd>!</kbd> | Suggestion list for actions |
| <kbd>Tab</kbd> | Accept the suggestion |
| <kbd>Esc</kbd> | Close the suggestion list |

**Timeline only:**

| Key | Effect |
|:--|:--|
| <kbd>Ctrl</kbd>+wheel | Zoom |
| <kbd>Shift</kbd>+wheel | Scroll horizontally |

The list is also in the program under **Help → Keyboard shortcuts**.

---

## 11.7 Where the settings live

```
  %APPDATA%/StoryEditor/
  ├── settings.json          language, theme, colours, open windows,
  │                          most recently used vault
  └── imgui_layout_v3.ini    window positions and sizes
```

These files belong to the **program**, not to the project – a vault therefore looks the
same on every machine.

**View → Reset layout** restores the original arrangement.

---

## ✅ In short

- Language and theme switch while running, with no risk to the project
- Every dropdown can be extended with **your own entries** and maintained under
  **Settings → Own lists**
- Autosave writes after a short pause; the indicator on the right tells you the state
- **64 steps** of undo, covering the whole project

---

[<kbd> &nbsp; ← Overview &nbsp; </kbd>](../../README.en.md)
[<kbd> &nbsp; ← Back: 10 · Files, images & export &nbsp; </kbd>](10-files.md)
[<kbd> &nbsp; Next: 12 · Under the hood → &nbsp; </kbd>](12-internals.md)

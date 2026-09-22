[<kbd> &nbsp; ← Overview &nbsp; </kbd>](../../README.en.md)
[<kbd> &nbsp; ← Back: 9 · Chronicle & detail panel &nbsp; </kbd>](09-chronicle.md)
[<kbd> &nbsp; Next: 11 · Settings & controls → &nbsp; </kbd>](11-settings.md)

---

# 💾 Chapter 10 · Files, images & export

> **What is this about?** Getting images into the story, and getting the finished story
> back out – as a Word file or to read in Obsidian.

---

# Part 1 · The file manager

**Windows → File manager**

## 10.1 Two views

```
  ┌─ Structured ────────────┐     ┌─ Raw files ─────────────┐
  │                         │     │                         │
  │  📁 Characters          │     │  📁 Actions             │
  │    📁 Main              │     │  📁 Assets              │
  │      📄 Alice.md        │     │  📁 Characters          │
  │      🖼️ alice.png        │     │  📁 Connections         │
  │      📄 Bob.md          │     │  📁 Locations           │
  │  📁 Locations           │     │  📁 Manuscript          │
  │      📄 Castle.md       │     │  📁 Objects             │
  │                         │     │  📄 metadata.json       │
  │  the way the program    │     │  the way it really      │
  │  sorts your world       │     │  sits on disk           │
  └─────────────────────────┘     └─────────────────────────┘
```

| View | What for |
|:--|:--|
| **Structured** | Sorted by your groups. The normal route |
| **Raw files** | The actual folder contents, including `metadata.json` and `Assets/` |

## 10.2 What you can do here

| Button / menu | Effect |
|:--|:--|
| **Upload file…** | File picker; the file is **copied into the vault** |
| **Drag & drop** | Drag a file from Explorer into the program window |
| **New folder…** | Create a sub folder |
| **Open vault** | Show the vault in Windows Explorer |
| Right click → **Open** | Show an image resp. the raw text |
| Right click → **Rename** | |
| Right click → **Move to…** | Target folder relative to the vault |
| Right click → **Delete** | With a confirmation |
| Right click → **Link with element** | See below |

> 📌 Uploaded files are **copied**, not linked. That keeps the vault self-contained –
> you can carry it to another machine.

## 10.3 Attaching an image to a character

```
  1. Upload the image      →  lands in  Assets/Images/alice.png
  2. Right click → "Link with element"
  3. Pick the element      →  Alice
  4. Done                  →  Alice.md now carries the reference,
                              the Details window shows a preview
```

Alternatively, put a field of type **File** into the template
(→ [Chapter 4](04-groups.md)) – then every character has a portrait slot by default.

## 10.4 Preview

| File type | View |
|:--|:--|
| 🖼️ **Images** (png, jpg, …) | Thumbnail, click shows it large |
| 📄 **Markdown/text** | Raw contents, **view only** |
| 📦 **Everything else** | Name and size only |

> 🔴 The same rule applies here: do **not** edit `.md` files by hand. That is why the
> file manager deliberately only displays them.

---

# Part 2 · Getting it out

## 10.5 As a Word file

**Manuscript → File → As Word**
or **File → Manuscript as Word (.docx)…**

What comes out:

| In the manuscript | In the Word file |
|:--|:--|
| Project name | Title on the first page – unless the manuscript has its own title (`# …`) |
| `# Title` | Title |
| `## Chapter 1` | Heading 1 |
| `### Scene` | Heading 2 |
| `@Alice` | `Alice` |
| `@Alice.age` | The number belonging to that spot |
| Bold, italic, underline, strikethrough, super‑/subscript | The same formatting in Word |
| Font, size, colour, highlight | The same formatting in Word |
| Alignment, bullets, numbering | The same formatting in Word |
| `---` | Centred separator `* * *` |
| `#Day 5, 14:00` | **gone** |
| `!act:…` | **gone** |
| Bookmarks, comments | **gone** |
| Every line | One paragraph – just like on the page in the editor |
| Base font, line spacing, margins, paper size | From **Layout** |

The file is a normal `.docx` and opens in Word, LibreOffice or Google Docs. You do not
need Word installed – the program builds the package itself.

## 10.6 Reading on in Obsidian

On every save the program writes **two** versions of your text:

```
  Manuscript/
    manuscript.md   ← your working text, with all markers
                       (the program reads and writes this file)

    gelesen.md      ← the reading version: values inserted, markers gone,
                       bold/italic as plain Markdown
                       (just for you to read)
```

Open the vault folder in Obsidian and you can read and search `gelesen.md` like any
other note. The element files (`Alice.md`) are normal notes too, with working
`[[wiki links]]`.

## 10.7 What else is generated

| File | Contents |
|:--|:--|
| `Timeline/events.md` | All events chronologically, as a reading overview |
| `Alice.md` → `## Beziehungen` | Generated from the connections |
| `Alice.md` → `## Verknuepfte Aktionen` | Generated from the actions |

These sections are **rewritten** on every save – changes inside them are lost. From an
element file only `## Felder` and `## Text` are read back.

---

## ✅ In short

- The **file manager** brings images into the vault and links them to elements
- Files are **copied**, so the vault stays self-contained
- **Save as Word** produces a finished manuscript with headings and formatting
- `gelesen.md` is the reading version for Obsidian – your working file stays `manuscript.md`

---

[<kbd> &nbsp; ← Overview &nbsp; </kbd>](../../README.en.md)
[<kbd> &nbsp; ← Back: 9 · Chronicle & detail panel &nbsp; </kbd>](09-chronicle.md)
[<kbd> &nbsp; Next: 11 · Settings & controls → &nbsp; </kbd>](11-settings.md)

[<kbd> &nbsp; ← Overview &nbsp; </kbd>](../../README.en.md)
[<kbd> &nbsp; ← Back: 2 · The program window &nbsp; </kbd>](02-overview.md)
[<kbd> &nbsp; Next: 4 · Characters, places, things → &nbsp; </kbd>](04-groups.md)

---

# ✍️ Chapter 3 · Writing

> **The most important chapter.** This is where the actual work happens. The manuscript
> looks and works like Word: a sheet of paper, the ribbon above it, the status bar below.
> By the end you can write and format text, insert characters and values, structure,
> attach notes and search – **without memorising a single special character.**

---

## 3.1 First of all: just write

![The manuscript window](../bilder/manuskript-seite.png)

Click on the page and start typing. What you see is what you get: headings are big,
bold is bold, colours are coloured.

```
 Day 1. Alice came back, and the town was smaller than she
 remembered it. Bob was standing at the market, and did not
 recognise her at first.
```

| Key | Effect |
|:--|:--|
| <kbd>Enter</kbd> | New paragraph (in a list: new item; on an empty item: end the list) |
| <kbd>←</kbd> <kbd>→</kbd> <kbd>↑</kbd> <kbd>↓</kbd>, <kbd>Home</kbd> <kbd>End</kbd>, <kbd>PgUp</kbd> <kbd>PgDn</kbd> | Move the cursor – with <kbd>Shift</kbd> to select, with <kbd>Ctrl</kbd> word by word |
| Double / triple click | Select a word / the whole paragraph |
| <kbd>Ctrl</kbd>+<kbd>A</kbd> | Select everything |
| <kbd>Ctrl</kbd>+<kbd>X</kbd> / <kbd>C</kbd> / <kbd>V</kbd> | Cut, copy, paste – inside the manuscript the formatting is kept |
| <kbd>Ctrl</kbd>+<kbd>Z</kbd> / <kbd>Y</kbd> | Undo / redo |
| <kbd>Ctrl</kbd>+mouse wheel | Zoom |
| Right click | Context menu with the most common commands |

That already works. But the program knows nothing about Alice and Bob while you do it –
to it they are just letters. Section 3.4 changes that.

---

## 3.2 The ribbon

![The tabs](../bilder/manuskript-register.png)

Top left are **Save**, **Undo** and **Redo**, next to them the tabs. The little arrow on
the far right collapses and expands the ribbon.

| Tab | What it holds |
|:--|:--|
| **File** | Save, save as Word, help, settings |
| **Home** | Clipboard, font, paragraph, styles, find |
| **Insert** | Characters, values, actions, points in time, scene breaks, bookmarks, comments |
| **Layout** | Margins, paper size, line spacing, base font, pages or continuous |
| **Review** | Spelling, AutoCorrect, language, comments, bookmarks, word count |
| **View** | Read mode, ruler, navigation pane, zoom, split view, focus |

> 💡 The program remembers which tab was open last.

---

## 3.3 Formatting

![The Home tab](../bilder/manuskript-format.png)

Like in Word: **select first, then click.** Without a selection the format applies to
what you type next.

### Font

| Button | Effect |
|:--|:--|
| **Georgia ▾** | Font of the selection |
| **12 ▾** | Font size in points – type it or pick from the list |
| **A˄ A˅** | One step bigger / smaller |
| **A with eraser** | Clear formatting |
| **B I U ab** | Bold, italic, underline, strikethrough |
| **x₂ x²** | Subscript and superscript |
| **Pen ▾** | Text highlight (marker colour) |
| **A ▾** | Font colour – from the palette or freely chosen under "More colours…" |

The buttons show what applies at the cursor – with the cursor in bold text, **B** is
highlighted.

### Paragraph

| Button | Effect |
|:--|:--|
| **•≡** / **1≡** | Bullets / numbering – the numbers count on by themselves |
| **Alignment** | Left, centre, right, justify |
| **Line spacing ▾** | 1.0 to 3.0 – applies to the whole manuscript |
| **¶** | Show time and action markers in read mode too |
| **✱✱✱** | Insert a scene break |

### Styles

| Style | What for | In Word |
|:--|:--|:--|
| **Normal** | Body text | Normal |
| **Title** | Title of the work | Title |
| **Chapter** | Chapter heading – shows in the navigation pane | Heading 1 |
| **Scene** | Scene heading – indented in the navigation pane | Heading 2 |

One click formats the whole paragraph the cursor is in.

### Keyboard shortcuts

The shortcuts follow the German keyboard, like the German Word:

| Shortcut | Effect |
|:--|:--|
| <kbd>Ctrl</kbd>+<kbd>B</kbd> / <kbd>I</kbd> / <kbd>U</kbd> | Bold / italic / underline |
| <kbd>Ctrl</kbd>+<kbd>+</kbd> / <kbd>Ctrl</kbd>+<kbd>#</kbd> | Superscript / subscript |
| <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>></kbd> / <kbd>Ctrl</kbd>+<kbd><</kbd> | Grow / shrink font (US keyboards: <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>.</kbd> / <kbd>,</kbd>) |
| <kbd>Ctrl</kbd>+<kbd>Space</kbd> | Clear formatting |
| <kbd>Ctrl</kbd>+<kbd>L</kbd> / <kbd>E</kbd> / <kbd>R</kbd> / <kbd>J</kbd> | Left / centre / right / justify |

> 🛟 **No chaos possible:** the program writes the formatting into the file itself and
> always keeps it complete – half a highlight that turns the rest of the book bold cannot
> happen.

---

## 3.4 Inserting a character

Instead of just typing "Alice", you place a **marker**. Then the program knows that
*the* Alice is meant here.

![The Insert tab](../bilder/manuskript-einfuegen.png)

### How to

1. Put the cursor where the name should go
2. **Insert → Element ▾**
3. Type into the search field at the top if the list is long
4. Click the name

The text now shows `@Alice` – with a coloured background in the colour of her group.
If a word follows directly, the program puts a space in between.

### Faster: just type `@`

When you are in the flow, simply type `@` in the middle of a sentence and keep typing the
name. A list of suggestions opens below the cursor:

| Key | Effect |
|:--|:--|
| <kbd>↑</kbd> <kbd>↓</kbd> | Choose a suggestion |
| <kbd>Enter</kbd> | Accept (when something is chosen) |
| <kbd>Tab</kbd> | Accept, always |
| <kbd>Esc</kbd> | Dismiss the list |

> 💡 <kbd>Enter</kbd> still makes a new paragraph as long as you have not chosen
> anything. Just clicking into a marker does not open the list – it appears once you type.

### What you get out of it

| | |
|:--|:--|
| 🎨 **Visible** | The marker has the background of the group colour |
| 🔗 **Clickable** | <kbd>Ctrl</kbd>+click opens the character in the Details window, hovering shows the path |
| 🔴 **Typo‑proof** | A name that does not exist turns **red** – you see the mistake at once |
| 📊 **Useful** | The timeline now knows that Alice appears in this scene |

---

## 3.5 Inserting a value

This is the trick the program exists for.

Instead of "Alice was 27 years old" you write **"Alice was `@Alice.age` years old"**.
The finished text then automatically shows the number that applies *at this point in
the story* – 27 in chapter 1, 28 in chapter 9.

### How to

1. **Insert → Value ▾**
2. Point at the character – a submenu opens with **all their fields**
3. Next to each field is the value that applies right here
4. Click the field

```
  Insert ▸ Value ▸ ● Alice ▸ ┌───────────────────────────┐
                             │ name       Alice          │
                             │ age        27             │
                             │ status     Alive          │
                             │ backstory  (empty)        │
                             └───────────────────────────┘
```

The text now shows `@Alice.age`; hovering shows the current value. If the field is empty
or does not exist, the program uses the name instead – so there is never a gap in the text.

> 📖 Why the same expression yields two different numbers is explained in
> **[Chapter 5 · How time works](05-time.md)**.

---

## 3.6 Bookmarks and comments

Two tools just for you – they never appear in the finished text or the Word file.

| | How | In the text |
|:--|:--|:--|
| 🚩 **Bookmark** | **Insert → Bookmark** (or **Review**), enter a name | A flag; the name shows in the navigation pane, **Review → Go to** jumps there |
| 💬 **Comment** | **Insert → Comment**, **Review → New comment** or right click | A speech bubble; hovering shows the text, clicking edits or deletes it |

With **Review → Previous / Next** you walk through all comments in order. When one is
selected, you can edit and delete it there as well.

---

## 3.6a Spelling and AutoCorrect

Like in Word: unknown words get a **red wave**. The word you are typing stays clean until
you have finished it.

| | |
|:--|:--|
| **Right click** on a red word | Suggestions – one click puts them in. Plus **Ignore all** (until restart) and **Add to dictionary** (permanent) |
| <kbd>F7</kbd> or **Review → Next error** | Jumps to the next unknown word and selects it |
| **Review → Spelling** | Turn the waves on and off |
| **Review → AutoCorrect** | Turn correcting while typing on and off |
| **Language** (ribbon or status bar) | **German** or **English** – "Variants" also offers e.g. English (UK) or German (Switzerland) |

**AutoCorrect** kicks in as soon as a word is finished:

| You type | It becomes |
|:--|:--|
| `this is good. here` | `This is good. Here` – sentence start capitalised (not after "e.g.", "etc." …) |
| `THis` | `This` |
| `"Hello"` · `it's` | `“Hello”` · `it’s` (German: „Hallo“) |
| `...` · `word - word` | `…` · `word – word` |
| known typos | the correction from the Windows list |

Don't like a correction? Press <kbd>Ctrl</kbd>+<kbd>Z</kbd> right afterwards – only the
correction goes away, what you typed stays.

> 💡 The dictionaries come from **Windows** – the same as in Edge. Which languages exist is
> set under *Settings → Time & language → Language & region*. If one is missing, the
> status bar shows the language name in yellow. The names of your characters, places and
> things count as correct automatically; `@` markers are never checked. Your own words
> live in `woerterbuch.txt` in the settings folder – your Windows dictionary is untouched.

---

## 3.7 The navigation pane

On the left is the outline of your text: title and chapters in bold, scenes indented,
plus every point in time and every bookmark. A click jumps to the spot. Below it, the
list of all **comments** folds out.

Show or hide it via **View → Navigation pane**.

---

## 3.8 Find and replace

![The search bar](../bilder/manuskript-suchen.png)

**Home → Find** (<kbd>Ctrl</kbd>+<kbd>F</kbd>) or **Replace** (<kbd>Ctrl</kbd>+<kbd>H</kbd>)
shows a bar above the page.

| | |
|:--|:--|
| **While typing** | *All* hits are highlighted in yellow and counted ("1 / 5") |
| <kbd>Enter</kbd> / <kbd>F3</kbd> | Jump to the next hit |
| <kbd>Shift</kbd>+<kbd>F3</kbd> | To the previous one |
| ▲ ▼ | The same with the mouse |
| **Case** | Match upper and lower case |
| **Replace** | Only the current hit |
| **Replace all** | All at once – can be undone with <kbd>Ctrl</kbd>+<kbd>Z</kbd> |
| <kbd>Esc</kbd> | Close the bar |

---

## 3.9 Page, zoom and view

| Where | What |
|:--|:--|
| **Layout → Margins / Size** | Margin (narrow to wide) and paper (A4, A5, Letter) |
| **Layout → Line spacing, base font, size** | For the whole manuscript – also in the Word file |
| **Layout / View → Pages or continuous** | Separate sheets like on paper, or one continuous sheet across the full width |
| **View → Ruler** | Centimetre ruler above the page – works with the mouse, see below |
| **View → Zoom** | 50 % to 300 %, "One page", "Page width" – or <kbd>Ctrl</kbd>+mouse wheel |
| **View → Split** | Two places of the same text one above the other, e.g. read chapter 1 at the top while writing chapter 12 below. Drag the divider to resize |
| **View → Focus** | Just the page – ribbon, ruler and navigation pane disappear |

### The ruler

```
   grey │ white (text area)                                   │ grey
 ───────┼──▽──────────L──────────L──────────────────────────△──┼───────
        │  △  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16  │
        │  ▭                                                   │
     margin     tab stops                            right indent
```

| What | Effect |
|:--|:--|
| **▽ top left** | First line indent – e.g. 1.25 cm for the classic novel indent |
| **△ bottom left** | Hanging indent: the paragraph moves in, the first line stays put |
| **▭ box below** | Moves the whole paragraph including the first line |
| **△ right** | Right indent |
| **Click on the ruler** | Sets a **tab stop** (L mark). Drag to move it, drag it down off the ruler to remove it |
| **Grey/white border** | Moves the left or right **page margin** (for the whole manuscript) |

Indents and tab stops apply to the paragraph at the cursor – or to all selected ones.
While dragging, a dashed line on the page shows where it goes and a tip shows the value
in centimetres. Markers snap to 0.25 cm; hold <kbd>Alt</kbd> for free positioning.
<kbd>Tab</kbd> jumps to the next tab stop – without your own stops, every 1.25 cm like in
Word (the small grey ticks at the bottom of the ruler). <kbd>Enter</kbd> carries indents
and tab stops over to the new paragraph.

The **status bar** at the bottom shows:

```
 Page 3 of 12 · 4,210 words · Time: Day 5, 14:00 · ● Saved      ⛶ 📖 ▤ 🌐  − ━━●━━ +  100 %
```

| Display | Meaning |
|:--|:--|
| **Page x of y** | Where the cursor is |
| **Words** | Click opens the statistics (pages, words, characters, paragraphs, chapters); with a selection "12 of 4,210 words" |
| **Time: …** | The story's point in time at the cursor – click to move time on from here |
| **Saved** | See 3.11 |
| right | Focus, read mode, pages, continuous and the zoom |

---

## 3.10 Reading instead of writing

![Read mode](../bilder/manuskript-lesen.png)

**View → Read mode** (or the book icon at the bottom right) shows the finished text:

| | Writing | Reading |
|:--|:--|:--|
| Names | as marker `@Alice` | filled in |
| Values | `@Alice.age` | the number, e.g. `27` |
| Time and action markers | visible | only with **¶ Marks** |
| Bookmarks, comments | visible | gone |
| Clicking | edits the text | a filled‑in name opens the character |

Formatting looks the same in both modes. The Word file contains the text exactly as read
mode shows it.

---

## 3.11 Saving

The status bar at the bottom always shows the state:

| Display | Meaning |
|:--|:--|
| 🟢 **Saved** | Everything is in the vault |
| 🟡 **[ Save ]** | There are changes that have not been written yet |

Normally you do not have to do anything: after a short pause in typing (five seconds at
the latest) the program writes on its own. If you switch off **View → Autosave** in the
main menu, it only happens on <kbd>Ctrl</kbd>+<kbd>S</kbd>, by clicking the yellow
button, via the disk icon at the top left, or when closing.

---

## 3.12 The built‑in help

![The help window](../bilder/manuskript-hilfe.png)

**File → Help** opens a short version of this chapter – handy when you are in the
middle of writing and cannot remember where something was.

---

## 📋 What ends up in the file

You do not need to remember this – the ribbon inserts it. But in case you take a look
(the file is `Manuscript/manuscript.md` in the vault). The formats are chosen so that
Obsidian displays them too:

| In the text | Meaning |
|:--|:--|
| `@Alice` | Reference to an element |
| `@Alice.age` | The field's value at this point in time |
| `@Characters/Main/Alice` | Full path – in case two elements share a name |
| `#Day 5, 14:00` | From here on this point in time applies |
| `# Title` · `## Chapter` · `### Scene` | Headings |
| `**bold**` `*italic*` `~~struck~~` | Emphasis |
| `<u>…</u>` `<sup>…</sup>` `<sub>…</sub>` | Underline, superscript, subscript |
| `<span style="color:#C00000;background:#FFFF00;font-size:14pt;font-family:Georgia">…</span>` | Colour, highlight, size, font |
| `- item` · `1. item` | Bullets, numbering |
| `…%%center%%` at the end of a line | Alignment (`center`, `right`, `justify`) |
| `…%%pf:left=1.5;first=1.25;tabs=5,8%%` at the end of a line | Paragraph format from the ruler: indents and tab stops in cm |
| `%%bm:Name%%` | Bookmark |
| `%%note:Text%%` | Comment |
| `---` | Scene break |
| `!act:…` | Linked event – invisible in the finished text |

Every line of the file is one paragraph on the page.

---

## ✅ In short

- Just start writing – it looks and feels like Word
- **Home** formats: select, then click – without a selection it applies to what you type next
- **Insert** places characters, values, events and points in time; typing `@` is the shortcut
- Bookmarks and comments are just for you, the **navigation pane** shows the outline
- **Layout** and **View** decide page, zoom, ruler, split view and focus
- **Read mode** shows the finished text, **File → Help** explains it all again briefly

---

[<kbd> &nbsp; ← Overview &nbsp; </kbd>](../../README.en.md)
[<kbd> &nbsp; ← Back: 2 · The program window &nbsp; </kbd>](02-overview.md)
[<kbd> &nbsp; Next: 4 · Characters, places, things → &nbsp; </kbd>](04-groups.md)

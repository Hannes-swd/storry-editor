[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 2 · Das Programmfenster &nbsp; </kbd>](02-ueberblick.md)
[<kbd> &nbsp; Weiter: 4 · Figuren, Orte, Dinge → &nbsp; </kbd>](04-gruppen.md)

---

# ✍️ Kapitel 3 · Schreiben

> **Das wichtigste Kapitel.** Hier passiert die eigentliche Arbeit. Am Ende kannst du
> Text schreiben, Figuren einsetzen, Werte einbauen, gliedern, hervorheben und suchen –
> **ohne ein einziges Sonderzeichen auswendig zu lernen.**

---

## 3.1 Erstmal: einfach schreiben

Das Manuskript ist ein ganz normales Textfeld. Klick hinein und tipp los.

```
 Tag 1. Alice kam zurück, und die Stadt war kleiner, als sie sie
 in Erinnerung hatte. Am Markt stand Bob, der sie zuerst nicht
 erkannte.
```

Das funktioniert. Aber das Programm weiß dabei nichts über Alice und Bob –
für es sind das nur Buchstaben. Der nächste Schritt ändert das.

---

## 3.2 Die Leiste

Alles, was über reinen Text hinausgeht, setzt die Leiste oben für dich ein.
Sie ist in vier Gruppen geteilt:

```
┌──── was du gerade tust ────┬──── was in den Text kommt ────┬─ Ansicht ─┬── wo du stehst ──┐
│                            │                               │           │                  │
│  ◉ Schreiben   ○ Lesen     │  Einfügen  Format  Suchen     │ Ansicht ? │ Zeit: Tag 5 …   │
│                            │                               │           │ 96 Wörter        │
│                            │                               │           │ ● Gespeichert    │
└────────────────────────────┴───────────────────────────────┴───────────┴──────────────────┘
```

| Gruppe | Wofür |
|:--|:--|
| **Schreiben / Lesen** | Umschalten zwischen Bearbeiten und der fertigen Lesefassung |
| **Einfügen** | Figuren, Werte, Ereignisse und Zeitpunkte in den Text setzen |
| **Format** | Kapitel, Szenen, Fett, Kursiv |
| **Suchen** | Stellen finden und ersetzen |
| **Ansicht** | Gliederung ein/aus, Marken zeigen, als Word speichern |
| **?** | Ein kleines Hilfefenster, das genau das hier zusammenfasst |
| **rechts** | Zeitpunkt an der Cursorstelle, Wortzahl, Speicherstand |

> 💡 Alles wird **an der Stelle eingesetzt, an der dein Cursor stand**. Nach dem Klick
> springt die Eingabe von selbst dorthin zurück – du kannst einfach weiterschreiben.

---

## 3.3 Eine Figur einsetzen

Statt „Alice" einfach hinzutippen, setzt du eine **Marke**. Dann weiß das Programm,
dass hier *die* Alice gemeint ist.

### So geht's

![Das Einfügen-Menü](../bilder/manuskript-einfuegen.png)

1. Cursor an die Stelle setzen, wo der Name hin soll
2. **Einfügen → Element (Figur, Ort, Ding)**
3. Oben ins Suchfeld tippen, wenn die Liste lang ist
4. Auf den Namen klicken

Im Text steht danach `@Alice` – mit einem farbigen Untergrund in der Farbe ihrer Gruppe.

### Schneller: einfach `@` tippen

Wenn du im Schreibfluss bist, tipp mitten im Satz einfach `@` und schreib den Namen
weiter. Unter dem Cursor klappt eine Vorschlagsliste auf:

| Taste | Wirkung |
|:--|:--|
| <kbd>↑</kbd> <kbd>↓</kbd> | Vorschlag auswählen |
| <kbd>Enter</kbd> | Übernehmen (wenn etwas ausgewählt ist) |
| <kbd>Tab</kbd> | Übernehmen, immer |
| <kbd>Esc</kbd> | Liste wegklicken |

> 💡 <kbd>Enter</kbd> macht weiterhin einen Absatz, solange du nichts ausgewählt hast.
> Nach einem Satzende wie „… vor der @Castle." kommst du also ganz normal in die
> nächste Zeile.

### Was du davon hast

| | |
|:--|:--|
| 🎨 **Sichtbar** | Die Marke hat den Untergrund der Gruppenfarbe – Figuren grün, Orte blau, Dinge orange |
| 🔗 **Klickbar** | Klick öffnet die Figur im Details‑Fenster |
| 🔴 **Tippfehlersicher** | Ein Name, den es nicht gibt, wird **rot** – du siehst den Fehler sofort |
| 📊 **Auswertbar** | Die Timeline weiß jetzt, dass Alice in dieser Szene vorkommt |

---

## 3.4 Einen Wert einsetzen

Das ist der Trick, für den es das Programm gibt.

Statt „Alice war 27 Jahre alt" schreibst du **„Alice war `@Alice.age` Jahre alt"**.
Im fertigen Text steht dann automatisch die Zahl, die *an dieser Stelle der Geschichte*
gilt – in Kapitel 1 die 27, in Kapitel 9 die 28.

### So geht's

1. **Einfügen → Wert eines Elements**
2. Auf die Figur zeigen – ein Untermenü klappt auf mit **allen ihren Feldern**
3. Neben jedem Feld steht gleich der Wert, der hier gerade gilt
4. Feld anklicken

```
  Einfügen ▸ Wert eines Elements ▸ ● Alice ▸ ┌───────────────────────────┐
                                             │ name       Alice          │
                                             │ age        27             │
                                             │ status     Alive          │
                                             │ backstory  (leer)         │
                                             └───────────────────────────┘
```

Im Text steht danach `@Alice.age`. Wenn das Feld leer ist oder nicht existiert, setzt
das Programm ersatzweise den Namen ein – es entsteht also nie eine Lücke im Text.

> 📖 Warum derselbe Ausdruck zwei verschiedene Zahlen ergibt, steht in
> **[Kapitel 5 · Wie die Zeit funktioniert](05-zeit.md)**.

---

## 3.5 Gliedern und hervorheben

![Das Format-Menü](../bilder/manuskript-format.png)

| Menüpunkt | Was es macht | Im Text |
|:--|:--|:--|
| **Kapitel** | Große Überschrift, erscheint links in der Gliederung | `## Neues Kapitel` |
| **Szene** | Kleinere Überschrift, eingerückt in der Gliederung | `### Neue Szene` |
| **Szenenwechsel** | Trennung mitten im Kapitel | `---` |
| **Fett** <kbd>Strg</kbd>+<kbd>B</kbd> | Hebt markierten Text hervor | `**so**` |
| **Kursiv** <kbd>Strg</kbd>+<kbd>I</kbd> | dito | `*so*` |

**Fett und Kursiv:** Markier erst ein Wort und drück dann <kbd>Strg</kbd>+<kbd>B</kbd> –
die Zeichen legen sich um die Markierung. Ohne Markierung landen beide Zeichen am
Cursor und du schreibst dazwischen weiter.

Im Schreibmodus siehst du einen zarten grauen Untergrund, der zeigt, wie weit die
Hervorhebung reicht. Im Lesemodus und in der Word‑Datei wird daraus **echtes Fett**.

> 🛟 **Kein Chaos möglich:** Eine vergessene Hervorhebung endet spätestens am Absatz –
> ein einzelnes Sternchen färbt also nie den Rest des Buches ein. Und ein `*` mitten
> im Satz („3 * 4") bleibt ganz normaler Text.

---

## 3.6 Die Gliederung links

Jedes Kapitel, jede Szene und jeder Zeitpunkt taucht links in der Gliederung auf.

```
 ┌──────────────────────┐
 │ Gliederung           │
 ├──────────────────────┤
 │ Kapitel 1 - Rückkehr │ ← Klick springt an die Stelle im Text
 │ - Tag 1, 09:00       │
 │ Kapitel 2 - Der Fund │
 │ - Tag 5, 14:00       │
 │ - Tag 11, 08:00      │
 └──────────────────────┘
```

Ausblenden kannst du sie über **Ansicht → Gliederung**.

---

## 3.7 Suchen und Ersetzen

![Die Suchleiste](../bilder/manuskript-suchen.png)

**Suchen** in der Leiste (oder <kbd>Strg</kbd>+<kbd>F</kbd>) blendet eine Leiste über
dem Text ein.

| | |
|:--|:--|
| **Beim Tippen** | *Alle* Treffer werden gelb hinterlegt und gezählt („1 / 5") |
| <kbd>Enter</kbd> / <kbd>F3</kbd> | Zum nächsten Treffer springen |
| <kbd>Umschalt</kbd>+<kbd>F3</kbd> | Zum vorherigen |
| ▲ ▼ | Dasselbe mit der Maus |
| **Gross/klein** | Groß‑ und Kleinschreibung beachten |
| **Ersetzen** | Nur den angesteuerten Treffer |
| **Alle ersetzen** | Alle auf einmal – lässt sich mit <kbd>Strg</kbd>+<kbd>Z</kbd> rückgängig machen |

Der Treffer, zu dem du springst, wird im Text markiert – du kannst also sofort
weiterschreiben.

---

## 3.8 Lesen statt schreiben

![Der Lesemodus](../bilder/manuskript-lesen.png)

Schalt oben auf **Lesen** um. Derselbe Text, aber:

| | Schreiben | Lesen |
|:--|:--|:--|
| Marken | `@Alice`, `#Tag 5, 14:00` sichtbar | verschwunden |
| Namen | als Marke | eingesetzt |
| Werte | `@Alice.age` | die Zahl, z. B. `27` |
| Fett/Kursiv | Sternchen sichtbar | echte Schrift |
| Überschriften | `## Kapitel 1` | groß und farbig |
| Klicken | Text bearbeiten | Marke anklicken öffnet die Figur |

Zur Orientierung kannst du dir die Zeit‑ und Ereignismarken einblenden lassen:
**Ansicht → Marken zeigen**. In der fertigen Word‑Datei stehen sie nie.

---

## 3.9 Speichern

Rechts in der Leiste siehst du immer den Stand:

| Anzeige | Bedeutung |
|:--|:--|
| 🟢 **Gespeichert** | Alles liegt im Vault |
| 🟡 **[ Speichern ]** | Es gibt Änderungen, die noch nicht geschrieben sind |

Normalerweise musst du nichts tun: nach einer kurzen Tipp‑Pause (spätestens nach fünf
Sekunden) schreibt das Programm von selbst. Wenn du **Ansicht → Automatisch speichern**
ausschaltest, passiert das nur noch auf <kbd>Strg</kbd>+<kbd>S</kbd>, per Klick auf den
gelben Knopf oder beim Schließen.

---

## 3.10 Die eingebaute Hilfe

![Das Hilfefenster](../bilder/manuskript-hilfe.png)

Das **?** in der Leiste öffnet eine Kurzfassung dieses Kapitels – praktisch, wenn du
mitten im Schreiben nicht weißt, wo etwas war.

---

## 📋 Was am Ende wirklich in der Datei steht

Du musst dir das nicht merken – die Leiste setzt es ein. Aber falls du mal reinschaust:

| Im Text | Bedeutung |
|:--|:--|
| `@Alice` | Verweis auf ein Element |
| `@Alice.age` | Wert des Feldes zu diesem Zeitpunkt |
| `@Characters/Main/Alice` | Voller Pfad – falls zwei Elemente gleich heißen |
| `#Tag 5, 14:00` | Ab hier gilt dieser Zeitpunkt |
| `## Kapitel 1` | Überschrift |
| `### Szene` | Unterüberschrift |
| `**fett**` `*kursiv*` | Hervorhebung |
| `---` | Szenenwechsel |
| `!act:…` | Verknüpftes Ereignis – im fertigen Text unsichtbar |

---

## ✅ Kurz gesagt

- Schreib einfach los – wie in jedem Textprogramm
- **Einfügen** setzt Figuren, Werte, Ereignisse und Zeitpunkte ein; **Format** gliedert und hebt hervor
- `@` tippen ist die Abkürzung für „Figur einsetzen"
- <kbd>Strg</kbd>+<kbd>F</kbd> findet Stellen, <kbd>Strg</kbd>+<kbd>B</kbd>/<kbd>I</kbd> hebt hervor
- **Lesen** zeigt den fertigen Text, das **?** erklärt alles nochmal kurz

---

[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 2 · Das Programmfenster &nbsp; </kbd>](02-ueberblick.md)
[<kbd> &nbsp; Weiter: 4 · Figuren, Orte, Dinge → &nbsp; </kbd>](04-gruppen.md)

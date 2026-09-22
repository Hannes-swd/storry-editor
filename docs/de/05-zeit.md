[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 4 · Figuren, Orte, Dinge &nbsp; </kbd>](04-gruppen.md)
[<kbd> &nbsp; Weiter: 6 · Ereignisse → &nbsp; </kbd>](06-aktionen.md)

---

# ⏳ Kapitel 5 · Wie die Zeit funktioniert

> **Das kürzeste und wichtigste Kapitel.** Wenn du nur eine Seite liest, dann diese.
> Ohne sie wirkt vieles im Programm willkürlich; mit ihr ergibt alles Sinn.

---

## 5.1 Die eine Regel

> ### ⏸️ Die Zeit läuft nicht von allein weiter.
>
> Sie bleibt stehen, bis **du** sie weiterstellst.

„Viele Jahre später" in den Text zu schreiben, ändert für das Programm **nichts**.
Nur eine **Zeitmarke** bewegt die Uhr.

```
   dein Text                                     was gilt
 ┌───────────────────────────────────────┐    ┌─────────────┐
 │ #Tag 1, 09:00                         │───▶│ Tag 1, 09:00│
 │ Alice kam zurück.                     │    │      ║      │
 │ Am Markt stand Bob.                   │    │      ║      │
 │                                       │    │      ║      │
 │ Viele Jahre später ging er zur Schule.│    │      ║  ←── ändert nichts!
 │                                       │    │      ║      │
 │ #Tag 11, 08:00                        │───▶│ Tag 11,08:00│
 │ Ein Jahr älter stand sie vor der Burg.│    │      ║      │
 └───────────────────────────────────────┘    └─────────────┘
```

---

## 5.2 Wo du siehst, wann du bist

Rechts in der Manuskript‑Leiste steht immer der Zeitpunkt, an dem die Geschichte
**dort steht, wo dein Cursor ist**:

```
                                    ┌──────────────────────┐
  … Ansicht  ?                      │ Zeit: Tag 5, 14:00   │  ← anklickbar
                                    └──────────────────────┘
```

Klick den Cursor an eine andere Stelle im Text – die Anzeige ändert sich mit.

---

## 5.3 Die Zeit weiterstellen

Klick auf die Anzeige (oder **Einfügen → Zeitpunkt weiterstellen**):

```
┌─ Zeitpunkt ──────────────────────────────────────────────┐
│                                                          │
│ Die Zeit läuft nicht von allein weiter: sie bleibt       │
│ stehen, bis du sie weiterstellst. Ab der Marke gilt      │
│ der neue Zeitpunkt für alles, was danach im Text kommt.  │
│                                                          │
│ Hier gilt gerade: Tag 5, 14:00                           │
│ ──────────────────────────────────────────────────────── │
│ Weiter um:                                               │
│   [ + 1 Stunde  ]  Tag 5, 15:00     ← das Ergebnis steht │
│   [ + 6 Stunden ]  Tag 5, 20:00        gleich daneben,   │
│   [ + 1 Tag     ]  Tag 6, 14:00        du musst nicht    │
│   [ + 3 Tage    ]  Tag 8, 14:00        rechnen           │
│   [ + 1 Woche   ]  Tag 12, 14:00                         │
│   [ + 1 Monat   ]  Tag 35, 14:00                         │
│ ──────────────────────────────────────────────────────── │
│ Oder fester Zeitpunkt:                                   │
│   [ Tag 12, 09:00        ]  [ Einfügen ]                 │
│   z.B. "Tag 5, 14:00" oder "Jahr 2, Monat 3, Tag 15"     │
└──────────────────────────────────────────────────────────┘
```

Eingefügt wird eine Marke wie `#Tag 12, 09:00`, die immer allein auf ihrer Zeile steht.
Im fertigen Text ist sie unsichtbar.

---

## 5.4 Warum das so praktisch ist

Weil du dann **einmal** schreibst und das Programm den Rest anpasst:

```
  Kapitel 1                              Kapitel 9
  #Tag 1, 09:00                          #Tag 400, 09:00
  "Alice war @Alice.age Jahre alt."      "Alice war @Alice.age Jahre alt."
                  │                                      │
                  ▼                                      ▼
            "war 27 Jahre alt"                    "war 28 Jahre alt"
```

Derselbe Satz, dieselbe Marke – zwei verschiedene Zahlen. Wenn du später entscheidest,
dass Alice doch mit 25 anfängt, änderst du **einen** Wert und der ganze Roman stimmt
wieder.

Wie ein Wert sich unterwegs ändert, steht in
[Kapitel 6 · Ereignisse & Werteänderungen](06-aktionen.md).

---

## 5.5 Der Kalender

Das Programm rechnet in einem einfachen, erfundenen Kalender – passend für Fantasy,
Science‑Fiction oder alles, was keinen echten Kalender braucht:

| | |
|:--|:--|
| 1 Tag | 24 Stunden |
| 1 Monat | 30 Tage |
| 1 Jahr | 12 Monate = 360 Tage |
| Startpunkt | Tag 1, 00:00 |

Intern wird alles als **Minuten seit Geschichtsbeginn** gespeichert. Deshalb kannst du
beliebig zwischen Schreibweisen wechseln.

### Was du eintippen darfst

| Eingabe | Ergibt |
|:--|:--|
| `Tag 5, 14:30` | Tag 5, 14:30 Uhr |
| `Tag 5` | Tag 5, 00:00 |
| `14:30` | Tag 1, 14:30 |
| `Jahr 2, Monat 3, Tag 15` | = Tag 435 |
| `Day 5, 14:30` | dasselbe – Englisch geht auch |

> 💡 Beide Sprachen werden beim **Lesen** immer akzeptiert. Ein Sprachwechsel macht also
> kein bestehendes Projekt kaputt.

---

## 5.6 Relative Zeitpunkte

Manchmal weißt du nicht das Datum, aber den Abstand: *„zwei Stunden nachdem Alice
ankommt"*. Ereignisse dürfen sich deshalb auf andere Ereignisse beziehen:

```
┌─ Aktion bearbeiten ────────────────────────────────┐
│ Titel:     Bob erzählt vom Drachen                 │
│                                                    │
│ ☑ Relativ zu einer anderen Aktion                  │
│   Bezug:   [ Alice trifft Bob am Markt  ▼ ]        │
│   Abstand: [ 20 ] Minuten danach                   │
│                                                    │
│   → ergibt: Tag 1, 11:20                           │
└────────────────────────────────────────────────────┘
```

Verschiebst du später das Bezugs‑Ereignis, wandert das abhängige automatisch mit.

---

## ✅ Kurz gesagt

- Die Zeit **bleibt stehen**, bis du sie mit einer Marke weiterstellst
- Rechts in der Manuskript‑Leiste steht immer, **wann** du gerade bist
- Klick darauf → weiterstellen, mit Schnellschritten und fertiger Rechnung
- Kalender: 30‑Tage‑Monate, 360‑Tage‑Jahre, intern Minuten
- Ereignisse dürfen auch **relativ** zueinander liegen

---

[<kbd> &nbsp; ← Übersicht &nbsp; </kbd>](../../README.md)
[<kbd> &nbsp; ← Zurück: 4 · Figuren, Orte, Dinge &nbsp; </kbd>](04-gruppen.md)
[<kbd> &nbsp; Weiter: 6 · Ereignisse → &nbsp; </kbd>](06-aktionen.md)

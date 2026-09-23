// Selbsttest fuer das Schreibfeld: Tippen, Formatieren, Loeschen, Listen,
// Undo - alles ohne Fenster. Laeuft mit "StoryEditor.exe --selftest" nach den
// Tests der Datenschicht. Eine eigene ImGui-Instanz sorgt fuer Uhrzeit und
// eine Zwischenablage, die die echte des Systems nicht anfasst.
#include "ui/EditingSelfTest.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>
#include <string>

#include "imgui.h"

#include "app/Platform.h"
#include "app/SpellCheck.h"
#include "core/Manuscript.h"
#include "ui/AutoCorrect.h"
#include "ui/DocumentView.h"
#include "ui/Editor.h"
#include "ui/Theme.h"

namespace se {
namespace {

struct Checks {
    std::ostringstream os;
    int passed = 0;
    int failed = 0;
    void check(bool ok, const std::string& what, const std::string& detail = std::string()) {
        if (ok) {
            ++passed;
            os << "[ ok ] " << what << "\n";
        } else {
            ++failed;
            os << "[FAIL] " << what;
            if (!detail.empty()) os << "   -> \"" << detail << "\"";
            os << "\n";
        }
    }
};

std::string g_clipboard;

// Frisches Dokument mit Text, Cursor am Ende.
struct Fixture {
    Editor ed;
    ManuscriptDoc doc;
    DocumentView view{"##test"};

    explicit Fixture(const std::string& text) {
        ed.project.loaded = true;
        Group& chars = ed.project.addGroup("Characters", "");
        ed.project.addElement("Alice", chars.id);
        ed.project.manuscript = text;
        doc.views = {&view};
        doc.sync(ed);
        view.setCaret(text.size());
    }
    const std::string& text() { return ed.project.manuscript; }
    size_t at(const std::string& needle) { return text().find(needle); }
    std::string rendered() { return renderManuscript(ed.project, text()); }
    TextStyle styleOf(const std::string& word) {
        for (const ManuscriptToken& t : parseManuscript(ed.project, text())) {
            if (t.kind == ManuscriptToken::Kind::Text && t.raw.find(word) != std::string::npos) return t.style;
        }
        return TextStyle();
    }
};

void testTypingAndStyles(Checks& c) {
    Fixture f("Hallo Welt");
    docops::typeText(f.doc, f.view, "!");
    c.check(f.text() == "Hallo Welt!", "edit: typing at the end", f.text());

    f.view.select(f.at("Welt"), f.at("Welt") + 4);
    docops::applyStyle(f.doc, f.view, [](TextStyle& s) { s.bold = true; });
    c.check(f.text() == "Hallo **Welt**!", "edit: bold on a selection", f.text());
    c.check(docops::selectionHas(f.doc, f.view, [](const TextStyle& s) { return s.bold; }),
            "edit: selection reports bold");
    c.check(docops::selectedPlainText(f.doc, f.view) == "Welt", "edit: selection stays on the same word",
            docops::selectedPlainText(f.doc, f.view));

    // hinter "Welt" weitertippen erbt fett
    f.view.setCaret(f.view.selEnd());
    docops::typeText(f.doc, f.view, "x");
    c.check(f.text() == "Hallo **Weltx**!", "edit: typing inherits the format on the left", f.text());
    docops::backspace(f.doc, f.view, false);
    c.check(f.text() == "Hallo **Welt**!", "edit: backspace removes one character", f.text());

    // Loeschen quer durch die Auszeichnung laesst keine Sternchen zurueck
    f.view.select(f.at("lt"), f.text().size());
    docops::deleteSelection(f.doc, f.view);
    c.check(f.text() == "Hallo **We**", "edit: deleting across markup keeps it balanced", f.text());

    // Vorgemerktes Format ohne Auswahl (Strg+I, dann tippen)
    f.view.setCaret(f.text().size());
    docops::applyStyle(f.doc, f.view, [](TextStyle& s) { s.italic = true; });
    docops::typeText(f.doc, f.view, "ab");
    c.check(f.rendered() == "Hallo Weab", "edit: pending format - text", f.rendered());
    const TextStyle ab = f.styleOf("ab");
    c.check(ab.bold && ab.italic, "edit: pending format applied to the typed text", f.text());

    // Unterstrichen + Farbe + Groesse ueber eine Auswahl
    f.view.select(f.at("Hallo"), f.at("Hallo") + 5);
    docops::applyStyle(f.doc, f.view, [](TextStyle& s) {
        s.underline = true;
        s.color = "#C00000";
        s.size = 16.0f;
    });
    const TextStyle hallo = f.styleOf("Hallo");
    c.check(hallo.underline && hallo.color == "#C00000" && hallo.size == 16.0f,
            "edit: underline, colour and size", f.text());
    c.check(f.rendered() == "Hallo Weab", "edit: visible text unchanged by formatting", f.rendered());

    // Format loeschen
    f.view.select(0, f.text().size());
    docops::applyStyle(f.doc, f.view, [](TextStyle& s) { s = TextStyle(); });
    c.check(f.text() == "Hallo Weab", "edit: clear formatting removes all markup", f.text());
}

void testParagraphs(Checks& c) {
    Fixture f("- eins");
    docops::newParagraph(f.doc, f.view);
    c.check(f.text() == "- eins\n- ", "para: enter continues the list", f.text());
    docops::typeText(f.doc, f.view, "zwei");
    c.check(f.text() == "- eins\n- zwei", "para: typing in the new item", f.text());
    docops::newParagraph(f.doc, f.view);
    docops::newParagraph(f.doc, f.view);
    c.check(f.text() == "- eins\n- zwei\n", "para: enter on an empty item ends the list", f.text());

    Fixture g("Titel");
    docops::setLineKind(g.doc, g.view, LineKind::Heading, 2);
    c.check(g.text() == "## Titel", "para: chapter heading", g.text());
    docops::setAlign(g.doc, g.view, LineAlign::Center);
    c.check(g.text() == "## Titel%%center%%", "para: centred heading", g.text());
    docops::newParagraph(g.doc, g.view);
    docops::typeText(g.doc, g.view, "Text");
    c.check(g.text() == "## Titel%%center%%\nText", "para: after a heading comes body text", g.text());
    g.view.setCaret(g.at("Titel"));
    docops::setLineKind(g.doc, g.view, LineKind::Body, 0);
    c.check(g.text() == "Titel%%center%%\nText", "para: heading back to body keeps alignment", g.text());

    // Backspace am Anfang eines Listenpunkts loest erst die Liste
    Fixture h("- Punkt");
    h.view.setCaret(h.at("Punkt"));
    docops::backspace(h.doc, h.view, false);
    c.check(h.text() == "Punkt", "para: backspace at a bullet removes the bullet", h.text());

    // Zeilen verbinden
    Fixture j("**eins**\nzwei");
    j.view.setCaret(j.at("zwei"));
    docops::backspace(j.doc, j.view, false);
    c.check(renderManuscript(j.ed.project, j.text()) == "einszwei", "para: backspace joins lines", j.text());
    Fixture k("eins\nzwei");
    k.view.setCaret(k.at("\n"));
    docops::deleteForward(k.doc, k.view, false);
    c.check(k.text() == "einszwei", "para: delete at the line end joins lines", k.text());

    // Enter mitten im fetten Wort: beide Haelften bleiben fett
    Fixture m("ein **fettes** Wort");
    m.view.setCaret(m.at("tes"));
    docops::newParagraph(m.doc, m.view);
    c.check(m.text() == "ein **fet**\n**tes** Wort", "para: enter splits the formatting cleanly", m.text());
}

void testMarkers(Checks& c) {
    // Zeitmarken sind unteilbar
    Fixture f("x\n#Tag 2\ny");
    f.view.setCaret(f.at("y"));
    docops::backspace(f.doc, f.view, false);
    c.check(f.text() == "x\ny", "marks: backspace removes a time line as a whole", f.text());

    Fixture g("Absatz");
    docops::insertOwnLine(g.doc, g.view, "#Tag 3");
    c.check(g.text() == "Absatz\n#Tag 3\n", "marks: time line after the paragraph", g.text());

    Fixture h("Text");
    docops::insertSource(h.doc, h.view, " !act:a1");
    docops::typeText(h.doc, h.view, "weiter");
    c.check(h.text() == "Text !act:a1 weiter", "marks: typing after an action keeps the id intact", h.text());

    Fixture k("Hier **fett und**");
    k.view.setCaret(k.at("und"));
    docops::insertReference(k.doc, k.view, "@Alice");
    const std::vector<ManuscriptToken> toks = parseManuscript(k.ed.project, k.text());
    bool boldAlice = false;
    for (const ManuscriptToken& t : toks) {
        if (t.kind == ManuscriptToken::Kind::Element && t.resolved && t.style.bold) boldAlice = true;
    }
    c.check(boldAlice, "marks: inserted element takes the surrounding format", k.text());

    // Elementmarken werden nur als Ganzes formatiert
    Fixture m("Sie sah @Alice an.");
    m.view.select(m.at("Ali") + 1, m.at("Ali") + 3);
    docops::applyStyle(m.doc, m.view, [](TextStyle& s) { s.italic = true; });
    c.check(m.text() == "Sie sah *@Alice* an.", "marks: formatting covers the whole reference", m.text());
}

void testUndoAndClipboard(Checks& c) {
    Fixture f("Anfang");
    docops::typeText(f.doc, f.view, " und");
    docops::typeText(f.doc, f.view, " Ende");
    f.view.select(0, 6);
    docops::applyStyle(f.doc, f.view, [](TextStyle& s) { s.bold = true; });
    c.check(f.text() == "**Anfang** und Ende", "undo: setup", f.text());
    f.doc.undo(&f.view);
    c.check(f.text() == "Anfang und Ende", "undo: formatting undone", f.text());
    f.doc.undo(&f.view);
    c.check(f.text() == "Anfang", "undo: quick typing is one step", f.text());
    f.doc.redo(&f.view);
    c.check(f.text() == "Anfang und Ende", "undo: redo", f.text());

    // Kopieren und Einfuegen innerhalb des Manuskripts behaelt das Format
    Fixture g("Das ist <u>wichtig</u>.");
    g.view.select(g.at("wichtig"), g.at("wichtig") + 7);
    docops::copy(g.doc, g.view);
    c.check(g_clipboard == "wichtig", "clipboard: plain text for other programs", g_clipboard);
    g.view.setCaret(g.text().size());
    docops::paste(g.doc, g.view);
    c.check(g.text() == "Das ist <u>wichtig</u>.<u>wichtig</u>", "clipboard: format kept inside the manuscript",
            g.text());
    // Fremder Text kommt als schlichter Text
    g_clipboard = "zwei\nZeilen";
    docops::paste(g.doc, g.view);
    c.check(g.rendered() == "Das ist wichtig.wichtigzwei\nZeilen", "clipboard: plain text with a line break",
            g.rendered());
}

// Setzen der Seite in einem echten (unsichtbaren) ImGui-Frame: Seiten,
// Zeilenumbruch, Cursorpositionen - und wie lange das bei einem ganzen Roman dauert.
void testLayout(Checks& c) {
    std::string novel;
    for (int chapter = 1; chapter <= 40; ++chapter) {
        novel += "## Kapitel " + std::to_string(chapter) + "\n#Tag " + std::to_string(chapter) + "\n";
        for (int para = 0; para < 60; ++para) {
            novel += "Sie ging **langsam** durch die <u>Stadt</u> und dachte an @Alice, die "
                     "schon lange fort war. Der Wind trug den Geruch von Regen heran, und "
                     "irgendwo schlug eine Glocke.\n";
        }
    }
    Fixture f(novel);
    DocOptions opt;
    opt.font = std::string();  // Oberflaechenschrift - im Test ist keine andere geladen

    auto frame = [&](Fixture& fx) {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(1200, 900);
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(1200, 900));
        ImGui::Begin("layout-test");
        const auto start = std::chrono::steady_clock::now();
        fx.view.draw(fx.doc, opt, ImVec2(1180, 860), {}, false);
        const auto stop = std::chrono::steady_clock::now();
        ImGui::End();
        ImGui::EndFrame();
        return std::chrono::duration<double, std::milli>(stop - start).count();
    };

    const double first = frame(f);
    c.check(f.view.pageCount() > 20, "layout: a long manuscript spans many pages",
            std::to_string(f.view.pageCount()));
    const double idle = frame(f);

    // Ein Zeichen in der Mitte tippen: nur diese Zeile wird neu gesetzt.
    f.view.setCaret(f.text().size() / 2);
    docops::typeText(f.doc, f.view, "x");
    const auto parseStart = std::chrono::steady_clock::now();
    f.doc.tokens();
    const double parse =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - parseStart).count();
    const double afterEdit = frame(f);
    c.check(afterEdit < first, "layout: an edit is cheaper than the first layout");
    c.os << "       Zeichen: " << novel.size() << ", Seiten: " << f.view.pageCount() << ", erstes Setzen: "
         << static_cast<int>(first) << " ms, ruhig: " << static_cast<int>(idle) << " ms, nach Tastendruck: "
         << static_cast<int>(afterEdit) << " ms (Parsen vorher: " << static_cast<int>(parse) << " ms)\n";

    // Cursor steht nach dem Tippen hinter dem neuen Zeichen und ist sichtbar.
    c.check(f.view.caretVisible() && f.text()[f.view.cursor - 1] == 'x', "layout: caret right after the typed character");

    // Kurzer Text: Umbruch und Positionen
    Fixture g("## Titel\nEin kurzer Absatz.\n\n- Punkt");
    const double small = frame(g);
    (void)small;
    c.check(g.view.pageCount() == 1, "layout: short text fits one page");
}

// Rechtschreibung ueber Windows - haengt davon ab, welche Sprachen installiert
// sind; fehlt eine, wird das nur vermerkt.
void testSpelling(Checks& c) {
    const bool de = spell::supported("de-DE");
    const bool en = spell::supported("en-US");
    c.os << "       Rechtschreibung verfuegbar: de-DE " << (de ? "ja" : "nein") << ", en-US "
         << (en ? "ja" : "nein") << ", installiert:";
    for (const std::string& l : spell::installedLanguages()) c.os << " " << l;
    c.os << "\n";
    if (de) {
        spell::setLanguage("de-DE");
        const std::vector<spell::Issue> issues = spell::check("Das ist ein Tset mit Umlauten: schön grün.");
        c.check(issues.size() == 1 && issues[0].begin == 12 && issues[0].end == 16,
                "spell: German typo found at the right bytes", std::to_string(issues.size()));
        const std::vector<std::string> sugg = spell::suggest("Tset");
        c.check(std::find(sugg.begin(), sugg.end(), "Test") != sugg.end(), "spell: German suggestion");
        spell::setKnownNames({"Zyrakel"});
        c.check(spell::isCorrect("Zyrakel"), "spell: element names count as correct");
    }
    if (en) {
        spell::setLanguage("en-US");
        c.check(!spell::isCorrect("recieve") && spell::isCorrect("receive"), "spell: English check");
    }
}

// Einzuege und Tabstopps aus dem Lineal: Datei, Bearbeiten, Seitenlayout.
void testRulerFormat(Checks& c) {
    Fixture f("Ein Absatz.");
    docops::setParagraphFormat(f.doc, f.view, [](ParagraphFormat& p) {
        p.left = 2.0f;
        p.first = -0.5f;
        p.tabs = {5.0f, 2.5f};
    });
    c.check(f.text() == "Ein Absatz.%%pf:left=2;first=-0.5;tabs=2.5,5%%", "ruler: paragraph format written", f.text());
    const std::vector<ManuscriptLine> lines = manuscriptLines(f.text());
    c.check(lines[0].format.left == 2.0f && lines[0].format.first == -0.5f && lines[0].format.tabs.size() == 2,
            "ruler: paragraph format read back");
    c.check(renderManuscript(f.ed.project, f.text()) == "Ein Absatz.", "ruler: marker invisible in the text");
    docops::setAlign(f.doc, f.view, LineAlign::Center);
    c.check(f.text() == "Ein Absatz.%%pf:align=center;left=2;first=-0.5;tabs=2.5,5%%",
            "ruler: alignment and indents together", f.text());
    docops::newParagraph(f.doc, f.view);
    c.check(manuscriptLines(f.text())[1].format.left == 2.0f, "ruler: enter keeps the indents", f.text());
    docops::setParagraphFormat(f.doc, f.view, [](ParagraphFormat& p) { p = ParagraphFormat(); });
    c.check(manuscriptLines(f.text())[1].format == ParagraphFormat(), "ruler: indents removed again", f.text());
    const std::vector<ManuscriptLine> legacy = manuscriptLines("Alt%%right%%");
    c.check(legacy[0].align == LineAlign::Right && legacy[0].contentEnd == 3, "ruler: old alignment markers still work");

    // Auf der Seite: Einzug verschiebt den Zeilenanfang, Tab springt zum Tabstopp.
    Fixture g("Ohne Einzug\nMit Einzug%%pf:left=2%%\na\tb%%pf:tabs=5%%");
    DocOptions opt;
    opt.font = std::string();
    auto caretX = [&](size_t pos) {
        g.view.setCaret(pos);
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(1200, 900);
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();
        ImGui::Begin("ruler-test");
        g.view.draw(g.doc, opt, ImVec2(1180, 860), {}, false);
        ImGui::End();
        ImGui::EndFrame();
        return g.view.caretScreenPos().x;
    };
    const float cm = 96.0f / 2.54f;
    caretX(0);  // erster Frame: ImGui kennt die Breite des Schreibfelds erst danach
    const float plain = caretX(0);
    const float indented = caretX(g.at("Mit"));
    c.check(std::fabs(indented - plain - 2.0f * cm) < 1.0f, "ruler: left indent moves the line",
            std::to_string(indented - plain));
    const float afterTab = caretX(g.at("b"));
    c.check(std::fabs(afterTab - plain - 5.0f * cm) < 1.0f, "ruler: tab jumps to the tab stop",
            std::to_string(afterTab - plain));
}

// AutoKorrektur: Zeichen fuer Zeichen tippen, wie es die Tastatur tut.
void typeWithAutoCorrect(Fixture& f, const std::string& text, const std::string& lang) {
    size_t i = 0;
    while (i < text.size()) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        const size_t n = ch < 0x80 ? 1 : (ch >> 5) == 6 ? 2 : (ch >> 4) == 14 ? 3 : 4;
        const std::string typed = autocorrect::transformTyped(f.text(), f.view.cursor, text.substr(i, n), lang);
        docops::typeText(f.doc, f.view, typed, true);
        autocorrect::afterTyped(f.doc, f.view, typed, lang);
        i += n;
    }
}

void testAutoCorrect(Checks& c) {
    spell::setLanguage("de-DE");
    Fixture f("");
    typeWithAutoCorrect(f, "das ist gut. hier geht es weiter ", "de-DE");
    c.check(f.text() == "Das ist gut. Hier geht es weiter ", "autocorrect: sentence starts capitalised", f.text());

    Fixture g("");
    typeWithAutoCorrect(g, "Er sagte \"Hallo\" und ging's an.", "de-DE");
    c.check(g.text() == "Er sagte \xE2\x80\x9EHallo\xE2\x80\x9C und ging\xE2\x80\x99s an.",
            "autocorrect: German quotation marks and apostrophe", g.text());

    Fixture h("");
    typeWithAutoCorrect(h, "She said \"hi\" ", "en-GB");
    c.check(h.text() == "She said \xE2\x80\x9Chi\xE2\x80\x9D ", "autocorrect: English quotation marks", h.text());

    Fixture k("");
    typeWithAutoCorrect(k, "Und dann... Pause - weiter ", "de-DE");
    c.check(k.text() == "Und dann\xE2\x80\xA6 Pause \xE2\x80\x93 weiter ", "autocorrect: ellipsis and dash", k.text());

    Fixture m("");
    typeWithAutoCorrect(m, "DIese Sache ist z.B. nicht neu ", "de-DE");
    c.check(m.text() == "Diese Sache ist z.B. nicht neu ", "autocorrect: two capitals, no capital after z.B.",
            m.text());

    Fixture u("");
    typeWithAutoCorrect(u, "hallo ", "de-DE");
    u.doc.undo(&u.view);
    c.check(u.text() == "hallo ", "autocorrect: undo takes back only the correction", u.text());

    // Rote Wellen: der Text wird im unsichtbaren Frame gesetzt und geprueft.
    Fixture w("## Kapitel\nDie **Strase** war leer, und niemmand sagte @Alice zu Zyrakel.");
    spell::setKnownNames({"Zyrakel"});
    DocOptions opt;
    opt.font = std::string();
    opt.spellCheck = true;
    opt.language = "de-DE";
    for (int pass = 0; pass < 2; ++pass) {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(1200, 900);
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();
        ImGui::Begin("spell-test");
        w.view.draw(w.doc, opt, ImVec2(1180, 860), {}, false);
        ImGui::End();
        ImGui::EndFrame();
    }
    std::string marked;
    for (const auto& issue : w.view.visibleSpellIssues())
        marked += w.text().substr(issue.first, issue.second - issue.first) + " ";
    c.check(marked == "Strase niemmand ", "spell: typos underlined, markup/elements/names skipped", marked);
    size_t b = 0, e = 0;
    c.check(docops::findSpellingError(w.doc, 0, &b, &e) && w.text().substr(b, e - b) == "Strase",
            "spell: next error (F7)");

    Fixture r("");
    typeWithAutoCorrect(r, "@alice geht. ", "de-DE");
    c.check(r.text() == "@alice geht. ", "autocorrect: element markers stay untouched", r.text());
}

}  // namespace

int runEditingSelfTest(const std::string& reportPath) {
    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    pio.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) { g_clipboard = text ? text : ""; };
    pio.Platform_GetClipboardTextFn = [](ImGuiContext*) { return g_clipboard.c_str(); };
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // keine imgui.ini neben der .exe
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;  // Schriften werden nur berechnet, nie gezeichnet
    const theme::Fonts savedFonts = theme::fonts();
    theme::fonts() = theme::Fonts();
    theme::fonts().regular = io.Fonts->AddFontDefault();

    Checks c;
    c.os << "\nSchreibfeld\n-----------\n";
    testTypingAndStyles(c);
    testParagraphs(c);
    testMarkers(c);
    testUndoAndClipboard(c);
    testLayout(c);
    testSpelling(c);
    testAutoCorrect(c);
    testRulerFormat(c);
    c.os << "---------------------\n" << c.passed << " ok, " << c.failed << " fehlgeschlagen\n";

    theme::fonts() = savedFonts;
    ImGui::DestroyContext(ctx);
    ImGui::SetCurrentContext(previous);

    if (!reportPath.empty()) {
        std::string existing;
        platform::readFile(reportPath, &existing);
        std::string err;
        platform::writeFile(reportPath, existing + c.os.str(), &err);
    }
    return c.failed == 0 ? 0 : 1;
}

}  // namespace se

#include "core/DocxExport.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <vector>

#include "app/Platform.h"
#include "core/Manuscript.h"

namespace se {
namespace {

// ------------------------------------------------------------------ ZIP
// Ein .docx ist ein ZIP. Wir speichern unkomprimiert (Methode 0) - Word liest
// das genauso, und wir sparen uns eine Abhaengigkeit auf zlib.

uint32_t crc32Of(const std::string& data) {
    static uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        ready = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (unsigned char ch : data) c = table[(c ^ ch) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void put16(std::string& out, uint16_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
}

void put32(std::string& out, uint32_t v) {
    put16(out, static_cast<uint16_t>(v & 0xFFFF));
    put16(out, static_cast<uint16_t>((v >> 16) & 0xFFFF));
}

struct ZipEntry {
    std::string name;
    size_t size = 0;
    uint32_t crc = 0;
    uint32_t offset = 0;
};

class Zip {
public:
    Zip() {
        std::time_t now = std::time(nullptr);
        std::tm tm {};
#if defined(_WIN32)
        localtime_s(&tm, &now);
#else
        tm = *std::localtime(&now);
#endif
        time_ = static_cast<uint16_t>((tm.tm_hour << 11) | (tm.tm_min << 5) | (tm.tm_sec / 2));
        date_ = static_cast<uint16_t>(((tm.tm_year - 80) << 9) | ((tm.tm_mon + 1) << 5) |
                                      tm.tm_mday);
    }

    void add(const std::string& name, const std::string& data) {
        ZipEntry e;
        e.name = name;
        e.size = data.size();
        e.crc = crc32Of(data);
        e.offset = static_cast<uint32_t>(out_.size());
        put32(out_, 0x04034B50u);  // local file header
        put16(out_, 20);           // version needed
        put16(out_, 0);            // flags
        put16(out_, 0);            // method: stored
        put16(out_, time_);
        put16(out_, date_);
        put32(out_, e.crc);
        put32(out_, static_cast<uint32_t>(e.size));
        put32(out_, static_cast<uint32_t>(e.size));
        put16(out_, static_cast<uint16_t>(name.size()));
        put16(out_, 0);  // extra
        out_ += name;
        out_ += data;
        entries_.push_back(e);
    }

    std::string finish() {
        const uint32_t dirStart = static_cast<uint32_t>(out_.size());
        for (const ZipEntry& e : entries_) {
            put32(out_, 0x02014B50u);  // central directory header
            put16(out_, 20);           // version made by
            put16(out_, 20);           // version needed
            put16(out_, 0);            // flags
            put16(out_, 0);            // method
            put16(out_, time_);
            put16(out_, date_);
            put32(out_, e.crc);
            put32(out_, static_cast<uint32_t>(e.size));
            put32(out_, static_cast<uint32_t>(e.size));
            put16(out_, static_cast<uint16_t>(e.name.size()));
            put16(out_, 0);  // extra
            put16(out_, 0);  // comment
            put16(out_, 0);  // disk
            put16(out_, 0);  // internal attrs
            put32(out_, 0);  // external attrs
            put32(out_, e.offset);
            out_ += e.name;
        }
        const uint32_t dirSize = static_cast<uint32_t>(out_.size()) - dirStart;
        put32(out_, 0x06054B50u);  // end of central directory
        put16(out_, 0);
        put16(out_, 0);
        put16(out_, static_cast<uint16_t>(entries_.size()));
        put16(out_, static_cast<uint16_t>(entries_.size()));
        put32(out_, dirSize);
        put32(out_, dirStart);
        put16(out_, 0);  // comment length
        return out_;
    }

private:
    std::string out_;
    std::vector<ZipEntry> entries_;
    uint16_t time_ = 0;
    uint16_t date_ = 0;
};

// ------------------------------------------------------------------ XML
std::string xmlEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 16);
    for (char ch : in) {
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default:
                // Steuerzeichen sind in XML nicht erlaubt, Tab ausgenommen.
                if (static_cast<unsigned char>(ch) < 0x20 && ch != '\t') break;
                out.push_back(ch);
        }
    }
    return out;
}

std::string isoNow() {
    std::time_t now = std::time(nullptr);
    std::tm tm {};
#if defined(_WIN32)
    gmtime_s(&tm, &now);
#else
    tm = *std::gmtime(&now);
#endif
    char buf[32] = {0};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

// Ein Textstueck mit gleichbleibendem Zeichenformat.
struct Run {
    std::string text;
    TextStyle style;
};

// Ein Absatz: jede Zeile des Manuskripts wird einer - so sieht die Word-Datei
// genauso aus wie die Seite im Editor.
struct Para {
    enum class Kind { Body, Heading, SceneBreak, Bullet, Numbered };

    Kind kind = Kind::Body;
    int headingLevel = 0;
    int number = 0;
    LineAlign align = LineAlign::Left;
    ParagraphFormat format;  // Einzuege und Tabstopps aus dem Lineal
    std::vector<Run> runs;
};

void addRun(std::vector<Run>& runs, const std::string& text, const TextStyle& style) {
    if (text.empty()) return;
    if (!runs.empty() && runs.back().style == style) {
        runs.back().text += text;
        return;
    }
    runs.push_back({text, style});
}

std::vector<Para> buildParagraphs(const Project& p, const std::string& text) {
    const std::vector<ManuscriptToken> tokens = parseManuscript(p, text);
    std::vector<Para> paras;
    size_t tokenIndex = 0;

    for (const ManuscriptLine& line : manuscriptLines(text)) {
        if (line.kind == LineKind::Time) continue;  // steuert nur die Werte
        Para para;
        para.align = line.align;
        para.format = line.format;
        para.number = line.number;
        switch (line.kind) {
            case LineKind::Break: {
                para.kind = Para::Kind::SceneBreak;
                para.align = LineAlign::Center;
                para.runs.push_back({"* * *", TextStyle()});
                paras.push_back(para);
                continue;
            }
            case LineKind::Heading:
                // "# Titel" wird Words Titel, "## Kapitel" Ueberschrift 1,
                // "### Szene" (und tiefer) Ueberschrift 2.
                para.kind = Para::Kind::Heading;
                para.headingLevel = line.level <= 1 ? 0 : line.level == 2 ? 1 : 2;
                addRun(para.runs, text.substr(line.contentBegin, line.contentEnd - line.contentBegin),
                       TextStyle());
                paras.push_back(para);
                continue;
            case LineKind::Bullet: para.kind = Para::Kind::Bullet; break;
            case LineKind::Numbered: para.kind = Para::Kind::Numbered; break;
            default: break;
        }

        // Alle Marken, die in den Inhalt der Zeile fallen
        while (tokenIndex < tokens.size() && tokens[tokenIndex].end <= line.contentBegin &&
               tokens[tokenIndex].begin < line.contentBegin)
            ++tokenIndex;
        for (size_t k = tokenIndex; k < tokens.size(); ++k) {
            const ManuscriptToken& t = tokens[k];
            if (t.begin >= line.contentEnd) break;
            if (t.end <= line.contentBegin) continue;
            switch (t.kind) {
                case ManuscriptToken::Kind::Text: {
                    const size_t b = std::max(t.begin, line.contentBegin);
                    const size_t e = std::min(t.end, line.contentEnd);
                    std::string piece = text.substr(b, e - b);
                    piece.erase(std::remove(piece.begin(), piece.end(), '\r'), piece.end());
                    addRun(para.runs, piece, t.style);
                    break;
                }
                case ManuscriptToken::Kind::Element:
                    addRun(para.runs, t.resolved ? p.displayName(t.targetId) : t.raw, t.style);
                    break;
                case ManuscriptToken::Kind::Value: {
                    std::string value = t.resolved ? p.valueAt(t.targetId, t.field, t.time) : t.raw;
                    if (t.resolved && value.empty()) value = p.displayName(t.targetId);
                    addRun(para.runs, value, t.style);
                    break;
                }
                default:
                    break;  // Formatzeichen, Aktionen, Lesezeichen, Kommentare
            }
        }
        paras.push_back(para);
    }
    return paras;
}

// Zentimeter -> Word-Einheit (1/20 Punkt)
int twips(float cm) { return static_cast<int>(std::lround(cm * 566.93f)); }

// "#C00000" -> "C00000"
std::string hexOf(const std::string& color) { return color.size() == 7 ? color.substr(1) : color; }

std::string runXml(const Run& run, const DocxOptions& options) {
    const TextStyle& s = run.style;
    std::string props;
    if (!s.font.empty() && s.font != options.font)
        props += "<w:rFonts w:ascii=\"" + xmlEscape(s.font) + "\" w:hAnsi=\"" + xmlEscape(s.font) +
                 "\" w:cs=\"" + xmlEscape(s.font) + "\"/>";
    if (s.bold) props += "<w:b/>";
    if (s.italic) props += "<w:i/>";
    if (s.strike) props += "<w:strike/>";
    if (!s.color.empty()) props += "<w:color w:val=\"" + hexOf(s.color) + "\"/>";
    if (s.size > 0.0f)
        props += "<w:sz w:val=\"" + std::to_string(static_cast<int>(s.size * 2.0f + 0.5f)) + "\"/>";
    if (!s.background.empty())
        props += "<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"" + hexOf(s.background) + "\"/>";
    if (s.underline) props += "<w:u w:val=\"single\"/>";
    if (s.superscript) props += "<w:vertAlign w:val=\"superscript\"/>";
    else if (s.subscript) props += "<w:vertAlign w:val=\"subscript\"/>";

    std::string out = "<w:r>";
    if (!props.empty()) out += "<w:rPr>" + props + "</w:rPr>";
    // Tabulatoren sind in Word ein eigenes Element.
    size_t start = 0;
    while (true) {
        const size_t tab = run.text.find('\t', start);
        const std::string piece = run.text.substr(start, tab == std::string::npos ? std::string::npos
                                                                                : tab - start);
        if (!piece.empty()) out += "<w:t xml:space=\"preserve\">" + xmlEscape(piece) + "</w:t>";
        if (tab == std::string::npos) break;
        out += "<w:tab/>";
        start = tab + 1;
    }
    out += "</w:r>";
    return out;
}

std::string paragraphXml(const Para& para, const DocxOptions& options) {
    std::string props;
    if (para.kind == Para::Kind::Heading)
        props += para.headingLevel == 0
                     ? std::string("<w:pStyle w:val=\"Title\"/>")
                     : "<w:pStyle w:val=\"Heading" + std::to_string(para.headingLevel) + "\"/>";
    // Reihenfolge wie im OOXML-Schema: Tabs, Abstand, Einzug, Ausrichtung
    const ParagraphFormat& pf = para.format;
    if (!pf.tabs.empty()) {
        props += "<w:tabs>";
        for (float t : pf.tabs) props += "<w:tab w:val=\"left\" w:pos=\"" + std::to_string(twips(t)) + "\"/>";
        props += "</w:tabs>";
    }
    if (para.kind == Para::Kind::SceneBreak)
        props += "<w:spacing w:before=\"240\" w:after=\"240\"/>";
    const bool list = para.kind == Para::Kind::Bullet || para.kind == Para::Kind::Numbered;
    const int left = twips(pf.left) + (list ? 720 : 0);
    const int right = twips(pf.right);
    const int first = twips(pf.first) - (list ? 360 : 0);  // Liste: haengender Einzug fuer das Zeichen
    if (left != 0 || right != 0 || first != 0) {
        props += "<w:ind w:left=\"" + std::to_string(left) + "\" w:right=\"" + std::to_string(right) + "\"";
        if (first > 0) props += " w:firstLine=\"" + std::to_string(first) + "\"";
        if (first < 0) props += " w:hanging=\"" + std::to_string(-first) + "\"";
        props += "/>";
    }
    switch (para.align) {
        case LineAlign::Center: props += "<w:jc w:val=\"center\"/>"; break;
        case LineAlign::Right: props += "<w:jc w:val=\"right\"/>"; break;
        case LineAlign::Justify: props += "<w:jc w:val=\"both\"/>"; break;
        default: break;
    }

    std::string out = "<w:p>";
    if (!props.empty()) out += "<w:pPr>" + props + "</w:pPr>";
    // Aufzaehlungszeichen als Text mit haengendem Einzug - das kommt ohne
    // eigene Nummerierungsdefinition aus und sieht in Word genauso aus.
    if (para.kind == Para::Kind::Bullet)
        out += runXml({"\xE2\x80\xA2\t", TextStyle()}, options);
    else if (para.kind == Para::Kind::Numbered)
        out += runXml({std::to_string(para.number) + ".\t", TextStyle()}, options);
    for (const Run& run : para.runs) out += runXml(run, options);
    out += "</w:p>";
    return out;
}

const char* kContentTypes =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
    "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
    "<Default Extension=\"rels\" "
    "ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
    "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
    "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-"
    "officedocument.wordprocessingml.document.main+xml\"/>"
    "<Override PartName=\"/word/styles.xml\" ContentType=\"application/vnd.openxmlformats-"
    "officedocument.wordprocessingml.styles+xml\"/>"
    "<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-"
    "package.core-properties+xml\"/>"
    "</Types>";

const char* kRootRels =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
    "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
    "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
    "relationships/officeDocument\" Target=\"word/document.xml\"/>"
    "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/"
    "relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
    "</Relationships>";

const char* kDocumentRels =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
    "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
    "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
    "relationships/styles\" Target=\"styles.xml\"/>"
    "</Relationships>";

// Formatvorlagen: Grundschrift, Zeilenabstand und Ueberschriften kommen aus
// den Einstellungen des Editors, damit Seite und Word-Datei gleich aussehen.
std::string stylesXml(const DocxOptions& o) {
    const std::string font = xmlEscape(o.font.empty() ? std::string("Georgia") : o.font);
    const int size = static_cast<int>(o.sizePt * 2.0f + 0.5f);
    const int line = static_cast<int>(o.lineSpacing * 240.0f + 0.5f);
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
           "<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
           "<w:docDefaults><w:rPrDefault><w:rPr>"
           "<w:rFonts w:ascii=\"" + font + "\" w:hAnsi=\"" + font + "\" w:cs=\"" + font + "\"/>"
           "<w:sz w:val=\"" + std::to_string(size) + "\"/>"
           "</w:rPr></w:rPrDefault>"
           "<w:pPrDefault><w:pPr><w:spacing w:after=\"0\" w:line=\"" + std::to_string(line) +
           "\" w:lineRule=\"auto\"/></w:pPr></w:pPrDefault></w:docDefaults>"
           "<w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\">"
           "<w:name w:val=\"Normal\"/></w:style>"
           "<w:style w:type=\"paragraph\" w:styleId=\"Title\"><w:name w:val=\"Title\"/>"
           "<w:basedOn w:val=\"Normal\"/><w:pPr><w:spacing w:after=\"360\"/></w:pPr>"
           "<w:rPr><w:b/><w:sz w:val=\"52\"/></w:rPr></w:style>"
           "<w:style w:type=\"paragraph\" w:styleId=\"Heading1\"><w:name w:val=\"heading 1\"/>"
           "<w:basedOn w:val=\"Normal\"/><w:pPr><w:keepNext/>"
           "<w:spacing w:before=\"360\" w:after=\"160\"/><w:outlineLvl w:val=\"0\"/></w:pPr>"
           "<w:rPr><w:b/><w:sz w:val=\"40\"/></w:rPr></w:style>"
           "<w:style w:type=\"paragraph\" w:styleId=\"Heading2\"><w:name w:val=\"heading 2\"/>"
           "<w:basedOn w:val=\"Normal\"/><w:pPr><w:keepNext/>"
           "<w:spacing w:before=\"240\" w:after=\"120\"/><w:outlineLvl w:val=\"1\"/></w:pPr>"
           "<w:rPr><w:b/><w:sz w:val=\"30\"/></w:rPr></w:style>"
           "</w:styles>";
}


}  // namespace

std::string buildManuscriptDocx(const Project& p, const std::string& title,
                               const DocxOptions& options) {
    // Jede Zeile wird ein Absatz: Werte sind eingesetzt, Zeit- und
    // Aktionsmarken, Lesezeichen und Kommentare fallen weg, die Formatierung
    // wird zu echten Word-Eigenschaften.
    std::string body;
    // Hat das Manuskript selbst einen Titel ("# ..."), ersetzt der den Projektnamen.
    const std::vector<Para> paras = buildParagraphs(p, p.manuscript);
    bool ownTitle = false;
    for (const Para& para : paras) {
        if (para.kind == Para::Kind::Heading && para.headingLevel == 0) ownTitle = true;
    }
    if (!title.empty() && !ownTitle)
        body += "<w:p><w:pPr><w:pStyle w:val=\"Title\"/></w:pPr><w:r>"
                "<w:t xml:space=\"preserve\">" +
                xmlEscape(title) + "</w:t></w:r></w:p>";
    for (const Para& para : paras) body += paragraphXml(para, options);
    if (body.empty()) body = "<w:p/>";

    const std::string document =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body>" +
        body +
        "<w:sectPr><w:pgSz w:w=\"" + std::to_string(twips(options.pageWidthCm)) + "\" w:h=\"" +
        std::to_string(twips(options.pageHeightCm)) + "\"/>"
        "<w:pgMar w:top=\"" + std::to_string(twips(options.marginCm)) + "\" w:right=\"" +
        std::to_string(twips(options.marginRightCm)) + "\" w:bottom=\"" +
        std::to_string(twips(options.marginCm)) + "\" w:left=\"" +
        std::to_string(twips(options.marginLeftCm)) + "\" "
        "w:header=\"708\" w:footer=\"708\" w:gutter=\"0\"/></w:sectPr>"
        "</w:body></w:document>";

    const std::string stamp = isoNow();
    const std::string core =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<cp:coreProperties "
        "xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
        "xmlns:dcterms=\"http://purl.org/dc/terms/\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
        "<dc:title>" +
        xmlEscape(title) +
        "</dc:title>"
        "<cp:lastModifiedBy>Story Editor</cp:lastModifiedBy>"
        "<dcterms:created xsi:type=\"dcterms:W3CDTF\">" +
        stamp +
        "</dcterms:created>"
        "<dcterms:modified xsi:type=\"dcterms:W3CDTF\">" +
        stamp +
        "</dcterms:modified>"
        "</cp:coreProperties>";

    Zip zip;
    zip.add("[Content_Types].xml", kContentTypes);
    zip.add("_rels/.rels", kRootRels);
    zip.add("docProps/core.xml", core);
    zip.add("word/_rels/document.xml.rels", kDocumentRels);
    zip.add("word/document.xml", document);
    zip.add("word/styles.xml", stylesXml(options));
    return zip.finish();
}

bool exportManuscriptDocx(const Project& p, const std::string& path, std::string* err,
                          const DocxOptions& options) {
    return platform::writeFile(path, buildManuscriptDocx(p, p.name, options), err);
}

}  // namespace se

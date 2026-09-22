#include "core/DocxExport.h"

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

// Ein Absatz: entweder Ueberschrift oder Fliesstext. Zeilenumbrueche innerhalb
// eines Absatzes bleiben als weiche Umbrueche erhalten.
struct Para {
    int headingLevel = 0;
    std::vector<std::string> lines;
};

std::string trimmed(const std::string& line) {
    const size_t b = line.find_first_not_of(" \t\r");
    if (b == std::string::npos) return std::string();
    const size_t e = line.find_last_not_of(" \t\r");
    return line.substr(b, e - b + 1);
}

std::vector<Para> splitParagraphs(const std::string& text) {
    std::vector<Para> paras;
    Para current;
    auto flush = [&]() {
        if (!current.lines.empty()) paras.push_back(current);
        current = Para();
    };

    size_t i = 0;
    while (true) {
        size_t lineEnd = text.find('\n', i);
        const bool last = lineEnd == std::string::npos;
        if (last) lineEnd = text.size();
        const std::string line = trimmed(text.substr(i, lineEnd - i));

        if (line.empty()) {
            flush();  // Leerzeile trennt Absaetze
        } else if (line[0] == '#') {
            flush();
            int level = 0;
            size_t k = 0;
            while (k < line.size() && line[k] == '#') {
                ++level;
                ++k;
            }
            while (k < line.size() && line[k] == ' ') ++k;
            // Im Manuskript ist "## Kapitel" die oberste Ueberschrift, darum
            // wird ein Rautenzeichen abgezogen.
            Para h;
            h.headingLevel = level >= 3 ? 2 : 1;
            h.lines.push_back(line.substr(k));
            if (!h.lines.front().empty()) paras.push_back(h);
        } else {
            current.lines.push_back(line);
        }

        if (last) break;
        i = lineEnd + 1;
    }
    flush();
    return paras;
}

std::string paragraphXml(const Para& para) {
    std::string out = "<w:p>";
    if (para.headingLevel > 0)
        out += "<w:pPr><w:pStyle w:val=\"Heading" + std::to_string(para.headingLevel) +
               "\"/></w:pPr>";
    out += "<w:r>";
    for (size_t i = 0; i < para.lines.size(); ++i) {
        if (i > 0) out += "<w:br/>";
        out += "<w:t xml:space=\"preserve\">" + xmlEscape(para.lines[i]) + "</w:t>";
    }
    out += "</w:r></w:p>";
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

const char* kStyles =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
    "<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
    "<w:docDefaults><w:rPrDefault><w:rPr>"
    "<w:rFonts w:ascii=\"Georgia\" w:hAnsi=\"Georgia\"/><w:sz w:val=\"24\"/>"
    "</w:rPr></w:rPrDefault>"
    "<w:pPrDefault><w:pPr><w:spacing w:after=\"160\" w:line=\"276\" w:lineRule=\"auto\"/>"
    "</w:pPr></w:pPrDefault></w:docDefaults>"
    "<w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\">"
    "<w:name w:val=\"Normal\"/></w:style>"
    "<w:style w:type=\"paragraph\" w:styleId=\"Title\"><w:name w:val=\"Title\"/>"
    "<w:basedOn w:val=\"Normal\"/><w:pPr><w:spacing w:after=\"360\"/></w:pPr>"
    "<w:rPr><w:b/><w:sz w:val=\"56\"/></w:rPr></w:style>"
    "<w:style w:type=\"paragraph\" w:styleId=\"Heading1\"><w:name w:val=\"heading 1\"/>"
    "<w:basedOn w:val=\"Normal\"/><w:pPr><w:keepNext/>"
    "<w:spacing w:before=\"480\" w:after=\"200\"/><w:outlineLvl w:val=\"0\"/></w:pPr>"
    "<w:rPr><w:b/><w:sz w:val=\"36\"/></w:rPr></w:style>"
    "<w:style w:type=\"paragraph\" w:styleId=\"Heading2\"><w:name w:val=\"heading 2\"/>"
    "<w:basedOn w:val=\"Normal\"/><w:pPr><w:keepNext/>"
    "<w:spacing w:before=\"360\" w:after=\"160\"/><w:outlineLvl w:val=\"1\"/></w:pPr>"
    "<w:rPr><w:b/><w:sz w:val=\"30\"/></w:rPr></w:style>"
    "</w:styles>";

}  // namespace

std::string buildManuscriptDocx(const Project& p, const std::string& title) {
    // renderManuscript setzt die Werte ein und laesst Zeit- und Aktionsmarken weg.
    const std::string rendered = renderManuscript(p, p.manuscript);

    std::string body;
    if (!title.empty())
        body += "<w:p><w:pPr><w:pStyle w:val=\"Title\"/></w:pPr><w:r>"
                "<w:t xml:space=\"preserve\">" +
                xmlEscape(title) + "</w:t></w:r></w:p>";
    for (const Para& para : splitParagraphs(rendered)) body += paragraphXml(para);
    if (body.empty()) body = "<w:p/>";

    const std::string document =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body>" +
        body +
        "<w:sectPr><w:pgSz w:w=\"11906\" w:h=\"16838\"/>"
        "<w:pgMar w:top=\"1417\" w:right=\"1417\" w:bottom=\"1417\" w:left=\"1417\" "
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
    zip.add("word/styles.xml", kStyles);
    return zip.finish();
}

bool exportManuscriptDocx(const Project& p, const std::string& path, std::string* err) {
    return platform::writeFile(path, buildManuscriptDocx(p, p.name), err);
}

}  // namespace se

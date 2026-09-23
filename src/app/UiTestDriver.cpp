#include "app/UiTestDriver.h"

#include <windows.h>
#include <d3d11.h>

#include <cstdint>
#include <sstream>
#include <vector>

#include "imgui.h"

#include "app/Platform.h"
#include "ui/Editor.h"

namespace se::uitest {
namespace {

struct State {
    bool loaded = false;
    bool ok = false;
    std::string dir;
    std::vector<std::string> lines;
    size_t next = 0;
    int wait = 0;
    bool done = false;
    std::string pendingShot;
    // mehrteilige Befehle (Klick, Taste): was im naechsten Frame folgt
    std::vector<std::string> followUps;
    std::string log;
};

State& state() {
    static State s;
    return s;
}

std::string scriptPath() {
    wchar_t buf[1024] = {0};
    if (GetEnvironmentVariableW(L"STORYEDITOR_UITEST", buf, 1024) == 0) return std::string();
    return platform::toUtf8(buf);
}

void load() {
    State& s = state();
    if (s.loaded) return;
    s.loaded = true;
    const std::string path = scriptPath();
    std::string text;
    if (path.empty() || !platform::readFile(path, &text)) return;
    const size_t slash = path.find_last_of("/\\");
    s.dir = slash == std::string::npos ? std::string(".") : path.substr(0, slash);
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (!line.empty() && line[0] != '#') s.lines.push_back(line);
    }
    s.ok = true;
}

std::string resolve(const std::string& file) {
    if (file.size() > 1 && (file[1] == ':' || file[0] == '/' || file[0] == '\\')) return file;
    return state().dir + "/" + file;
}

ImGuiKey keyFromName(const std::string& n) {
    if (n.size() == 1 && n[0] >= 'A' && n[0] <= 'Z') return static_cast<ImGuiKey>(ImGuiKey_A + (n[0] - 'A'));
    if (n.size() == 1 && n[0] >= 'a' && n[0] <= 'z') return static_cast<ImGuiKey>(ImGuiKey_A + (n[0] - 'a'));
    struct Named {
        const char* name;
        ImGuiKey key;
    };
    static const Named keys[] = {
        {"Enter", ImGuiKey_Enter},   {"Backspace", ImGuiKey_Backspace}, {"Delete", ImGuiKey_Delete},
        {"Tab", ImGuiKey_Tab},       {"Escape", ImGuiKey_Escape},       {"Left", ImGuiKey_LeftArrow},
        {"Right", ImGuiKey_RightArrow}, {"Up", ImGuiKey_UpArrow},       {"Down", ImGuiKey_DownArrow},
        {"Home", ImGuiKey_Home},     {"End", ImGuiKey_End},             {"F7", ImGuiKey_F7},
        {"Space", ImGuiKey_Space},
    };
    for (const Named& k : keys) {
        if (n == k.name) return k.key;
    }
    return ImGuiKey_None;
}

// ------------------------------------------------------------ PNG schreiben
uint32_t crc(const unsigned char* data, size_t len, uint32_t c = 0xFFFFFFFFu) {
    static uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t v = i;
            for (int k = 0; k < 8; ++k) v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            table[i] = v;
        }
        ready = true;
    }
    for (size_t i = 0; i < len; ++i) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c;
}

void put32(std::string& out, uint32_t v) {
    out += static_cast<char>((v >> 24) & 0xFF);
    out += static_cast<char>((v >> 16) & 0xFF);
    out += static_cast<char>((v >> 8) & 0xFF);
    out += static_cast<char>(v & 0xFF);
}

void chunk(std::string& out, const char* type, const std::string& data) {
    put32(out, static_cast<uint32_t>(data.size()));
    std::string body = std::string(type, 4) + data;
    out += body;
    put32(out, crc(reinterpret_cast<const unsigned char*>(body.data()), body.size()) ^ 0xFFFFFFFFu);
}

// Unkomprimiertes PNG (Deflate mit "stored"-Bloecken) - ohne zlib.
std::string encodePng(const std::vector<unsigned char>& rgba, unsigned w, unsigned h) {
    std::string raw;
    raw.reserve((w * 4 + 1) * h);
    for (unsigned y = 0; y < h; ++y) {
        raw += '\0';
        raw.append(reinterpret_cast<const char*>(&rgba[y * w * 4]), w * 4);
    }
    std::string z = "\x78\x01";
    size_t pos = 0;
    while (pos < raw.size() || raw.empty()) {
        const size_t len = std::min<size_t>(65535, raw.size() - pos);
        const bool last = pos + len >= raw.size();
        z += static_cast<char>(last ? 1 : 0);
        z += static_cast<char>(len & 0xFF);
        z += static_cast<char>((len >> 8) & 0xFF);
        z += static_cast<char>(~len & 0xFF);
        z += static_cast<char>((~len >> 8) & 0xFF);
        z.append(raw, pos, len);
        pos += len;
        if (last) break;
    }
    uint32_t a = 1, b = 0;
    for (unsigned char c : raw) {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    put32(z, (b << 16) | a);

    std::string png = "\x89PNG\r\n\x1A\n";
    std::string ihdr;
    put32(ihdr, w);
    put32(ihdr, h);
    ihdr += "\x08\x06\x00\x00\x00";
    chunk(png, "IHDR", std::string(ihdr.data(), 13));
    chunk(png, "IDAT", z);
    chunk(png, "IEND", std::string());
    return png;
}

void queue(const std::string& cmd) { state().followUps.push_back(cmd); }

// Fuehrt einen Befehl aus. Liefert, wie viele Frames danach zu warten ist.
int execute(Editor& ed, const std::string& line) {
    ImGuiIO& io = ImGui::GetIO();
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    if (cmd == "wait") {
        int n = 1;
        in >> n;
        return n;
    }
    if (cmd == "move") {
        float x = 0, y = 0;
        in >> x >> y;
        io.AddMousePosEvent(x, y);
        return 1;
    }
    if (cmd == "click" || cmd == "dblclick") {
        float x = 0, y = 0;
        std::string which;
        in >> x >> y >> which;
        const int button = which == "right" ? 1 : 0;
        io.AddMousePosEvent(x, y);
        io.AddMouseButtonEvent(button, true);
        queue("_up " + std::to_string(button));
        if (cmd == "dblclick") {
            queue("_down " + std::to_string(button));
            queue("_up " + std::to_string(button));
        }
        return 1;
    }
    if (cmd == "_down" || cmd == "_up") {
        int button = 0;
        in >> button;
        io.AddMouseButtonEvent(button, cmd == "_down");
        return 1;
    }
    if (cmd == "drag") {
        float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        in >> x1 >> y1 >> x2 >> y2;
        io.AddMousePosEvent(x1, y1);
        io.AddMouseButtonEvent(0, true);
        for (int i = 1; i <= 6; ++i)
            queue("move " + std::to_string(x1 + (x2 - x1) * i / 6.0f) + " " + std::to_string(y1 + (y2 - y1) * i / 6.0f));
        queue("_up 0");
        return 1;
    }
    if (cmd == "key") {
        std::string spec;
        in >> spec;
        bool ctrl = false, shift = false, alt = false;
        for (;;) {
            if (spec.rfind("ctrl+", 0) == 0) {
                ctrl = true;
                spec = spec.substr(5);
            } else if (spec.rfind("shift+", 0) == 0) {
                shift = true;
                spec = spec.substr(6);
            } else if (spec.rfind("alt+", 0) == 0) {
                alt = true;
                spec = spec.substr(4);
            } else {
                break;
            }
        }
        const ImGuiKey key = keyFromName(spec);
        if (ctrl) io.AddKeyEvent(ImGuiMod_Ctrl, true);
        if (shift) io.AddKeyEvent(ImGuiMod_Shift, true);
        if (alt) io.AddKeyEvent(ImGuiMod_Alt, true);
        if (key != ImGuiKey_None) io.AddKeyEvent(key, true);
        queue("_keyup " + std::to_string(static_cast<int>(key)) + " " + (ctrl ? "1" : "0") + (shift ? "1" : "0") +
              (alt ? "1" : "0"));
        return 1;
    }
    if (cmd == "_keyup") {
        int key = 0;
        std::string mods;
        in >> key >> mods;
        if (key != ImGuiKey_None) io.AddKeyEvent(static_cast<ImGuiKey>(key), false);
        if (mods.size() == 3) {
            if (mods[0] == '1') io.AddKeyEvent(ImGuiMod_Ctrl, false);
            if (mods[1] == '1') io.AddKeyEvent(ImGuiMod_Shift, false);
            if (mods[2] == '1') io.AddKeyEvent(ImGuiMod_Alt, false);
        }
        return 1;
    }
    if (cmd == "type") {
        const std::string text = line.size() > 5 ? line.substr(5) : std::string();
        io.AddInputCharactersUTF8(text.c_str());
        return 2;
    }
    if (cmd == "shot") {
        std::string file;
        in >> file;
        state().pendingShot = resolve(file);
        return 1;
    }
    if (cmd == "dump") {
        std::string file;
        in >> file;
        std::string err;
        platform::writeFile(resolve(file), ed.project.manuscript, &err);
        return 0;
    }
    if (cmd == "export") {
        std::string file;
        in >> file;
        ed.exportManuscriptToWord(resolve(file));
        return 1;
    }
    if (cmd == "quit") {
        state().done = true;
        return 0;
    }
    state().log += "unbekannter Befehl: " + line + "\n";
    return 0;
}

}  // namespace

bool active() { return !scriptPath().empty(); }

void beforeNewFrame(Editor& ed) {
    load();
    State& s = state();
    if (!s.ok || s.done) return;
    static bool focused = false;
    if (!focused) {
        ImGui::GetIO().AddFocusEvent(true);
        focused = true;
    }
    if (s.wait > 0) {
        --s.wait;
        return;
    }
    while (!s.done) {
        std::string line;
        if (!s.followUps.empty()) {
            line = s.followUps.front();
            s.followUps.erase(s.followUps.begin());
        } else if (s.next < s.lines.size()) {
            line = s.lines[s.next++];
        } else {
            s.done = true;
            break;
        }
        const int frames = execute(ed, line);
        if (frames > 0) {
            s.wait = frames - 1;
            break;
        }
    }
}

void afterRender(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapChain) {
    State& s = state();
    if (s.pendingShot.empty()) return;
    const std::string file = s.pendingShot;
    s.pendingShot.clear();

    ID3D11Texture2D* back = nullptr;
    if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&back))) || !back) return;
    D3D11_TEXTURE2D_DESC desc;
    back->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    ID3D11Texture2D* staging = nullptr;
    if (SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &staging)) && staging) {
        context->CopyResource(staging, back);
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
            std::vector<unsigned char> rgba(static_cast<size_t>(desc.Width) * desc.Height * 4);
            const bool bgra = desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM;
            for (UINT y = 0; y < desc.Height; ++y) {
                const unsigned char* src = static_cast<const unsigned char*>(mapped.pData) + y * mapped.RowPitch;
                unsigned char* dst = &rgba[static_cast<size_t>(y) * desc.Width * 4];
                for (UINT x = 0; x < desc.Width; ++x) {
                    dst[x * 4 + 0] = src[x * 4 + (bgra ? 2 : 0)];
                    dst[x * 4 + 1] = src[x * 4 + 1];
                    dst[x * 4 + 2] = src[x * 4 + (bgra ? 0 : 2)];
                    dst[x * 4 + 3] = 255;
                }
            }
            context->Unmap(staging, 0);
            std::string err;
            platform::writeFile(file, encodePng(rgba, desc.Width, desc.Height), &err);
        }
        staging->Release();
    }
    back->Release();
}

bool finished() { return state().done; }

}  // namespace se::uitest

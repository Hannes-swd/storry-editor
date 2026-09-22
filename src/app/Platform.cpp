#include "app/Platform.h"

#include <windows.h>

#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <fstream>
#include <sstream>

namespace se::platform {
namespace {

struct ComScope {
    bool ok = false;
    ComScope() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        ok = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    }
    ~ComScope() {
        if (ok) CoUninitialize();
    }
};

std::string runFileDialog(const char* title, bool pickFolders, bool save, const char* defaultName,
                          const char* filterLabel, const char* filterPattern,
                          std::vector<std::string>* multi) {
    ComScope com;
    IFileDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (FAILED(hr) || !dialog) return std::string();

    DWORD options = 0;
    dialog->GetOptions(&options);
    if (pickFolders) options |= FOS_PICKFOLDERS;
    if (multi) options |= FOS_ALLOWMULTISELECT;
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM);
    if (title) {
        std::wstring wtitle = toWide(title);
        dialog->SetTitle(wtitle.c_str());
    }
    if (defaultName) {
        std::wstring wname = toWide(defaultName);
        dialog->SetFileName(wname.c_str());
    }
    std::wstring wlabel, wpattern;
    COMDLG_FILTERSPEC spec{};
    if (filterLabel && filterPattern) {
        wlabel = toWide(filterLabel);
        wpattern = toWide(filterPattern);
        spec.pszName = wlabel.c_str();
        spec.pszSpec = wpattern.c_str();
        dialog->SetFileTypes(1, &spec);
    }

    std::string result;
    if (SUCCEEDED(dialog->Show(nullptr))) {
        if (multi) {
            IFileOpenDialog* open = nullptr;
            if (SUCCEEDED(dialog->QueryInterface(IID_PPV_ARGS(&open))) && open) {
                IShellItemArray* items = nullptr;
                if (SUCCEEDED(open->GetResults(&items)) && items) {
                    DWORD count = 0;
                    items->GetCount(&count);
                    for (DWORD i = 0; i < count; ++i) {
                        IShellItem* item = nullptr;
                        if (SUCCEEDED(items->GetItemAt(i, &item)) && item) {
                            PWSTR path = nullptr;
                            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                                multi->push_back(toUtf8(path));
                                CoTaskMemFree(path);
                            }
                            item->Release();
                        }
                    }
                    items->Release();
                }
                open->Release();
            }
            if (!multi->empty()) result = multi->front();
        } else {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                    result = toUtf8(path);
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
    }
    dialog->Release();
    return result;
}

}  // namespace

std::wstring toWide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), out.data(), len);
    return out;
}

std::string toUtf8(const std::wstring& wide) {
    if (wide.empty()) return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0,
                                  nullptr, nullptr);
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), out.data(), len,
                        nullptr, nullptr);
    return out;
}

std::filesystem::path fsPath(const std::string& utf8) { return std::filesystem::path(toWide(utf8)); }

std::string pathToUtf8(const std::filesystem::path& p) { return toUtf8(p.wstring()); }

bool readFile(const std::string& path, std::string* out) {
    std::ifstream in(fsPath(path), std::ios::binary);
    if (!in) return false;
    std::ostringstream os;
    os << in.rdbuf();
    *out = os.str();
    // tolerate a BOM written by external editors
    if (out->size() >= 3 && static_cast<unsigned char>((*out)[0]) == 0xEF &&
        static_cast<unsigned char>((*out)[1]) == 0xBB && static_cast<unsigned char>((*out)[2]) == 0xBF)
        out->erase(0, 3);
    return true;
}

bool writeFile(const std::string& path, const std::string& content, std::string* err) {
    std::filesystem::path p = fsPath(path);
    std::error_code ec;
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (err) *err = "Datei konnte nicht geschrieben werden: " + path;
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return true;
}

bool ensureDir(const std::string& path, std::string* err) {
    std::error_code ec;
    std::filesystem::create_directories(fsPath(path), ec);
    if (ec && !std::filesystem::exists(fsPath(path))) {
        if (err) *err = "Ordner konnte nicht erstellt werden: " + path;
        return false;
    }
    return true;
}

long long lastWriteTime(const std::string& path) {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(fsPath(path), ec);
    if (ec) return 0;
    return static_cast<long long>(t.time_since_epoch().count());
}

std::string pickFolder(const char* title) {
    return runFileDialog(title, true, false, nullptr, nullptr, nullptr, nullptr);
}

std::string pickFile(const char* title, const char* filterLabel, const char* filterPattern) {
    return runFileDialog(title, false, false, nullptr, filterLabel, filterPattern, nullptr);
}

std::vector<std::string> pickFiles(const char* title) {
    std::vector<std::string> files;
    runFileDialog(title, false, false, nullptr, nullptr, nullptr, &files);
    return files;
}

std::string saveFileDialog(const char* title, const char* defaultName) {
    return runFileDialog(title, false, true, defaultName, nullptr, nullptr, nullptr);
}

void openInShell(const std::string& path) {
    std::wstring w = toWide(path);
    ShellExecuteW(nullptr, L"open", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

std::string appConfigDir() {
    PWSTR roaming = nullptr;
    std::string dir;
    // STORYEDITOR_HOME: eigener Ordner fuer Einstellungen und Layout - fuer
    // Tests, ohne die echte Konfiguration anzufassen.
    wchar_t custom[1024] = {0};
    if (GetEnvironmentVariableW(L"STORYEDITOR_HOME", custom, 1024) > 0) {
        dir = toUtf8(custom);
    } else if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming)) && roaming) {
        dir = toUtf8(roaming) + "/StoryEditor";
        CoTaskMemFree(roaming);
    } else {
        dir = ".";
    }
    std::string err;
    ensureDir(dir, &err);
    return dir;
}

}  // namespace se::platform

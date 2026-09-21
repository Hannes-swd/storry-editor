// File manager (spec 3.6): structured view of the vault plus the raw folders.
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_stdlib.h"

#include "app/Platform.h"
#include "app/TextureCache.h"
#include "core/VaultIO.h"
#include "ui/Dialogs.h"
#include "ui/Editor.h"
#include "ui/Lang.h"
#include "ui/Theme.h"
#include "ui/UiCommon.h"
#include "ui/Windows.h"

namespace fs = std::filesystem;

namespace se {
namespace {

struct FileManagerState {
    bool rawMode = false;
    std::string currentDir;   // vault relative
    std::string selected;     // vault relative file
    std::string linkElementId;
    std::string previewText;
    std::string previewLoaded;
};

FileManagerState& state() {
    static FileManagerState s;
    return s;
}

std::string parentDir(const std::string& rel) {
    size_t slash = rel.find_last_of('/');
    if (slash == std::string::npos) return std::string();
    return rel.substr(0, slash);
}

std::string fileName(const std::string& rel) {
    size_t slash = rel.find_last_of('/');
    return slash == std::string::npos ? rel : rel.substr(slash + 1);
}

void copyInto(Editor& ed, const std::string& sourceAbs, const std::string& targetDirRel) {
    std::error_code ec;
    fs::path src = platform::fsPath(sourceAbs);
    std::string targetRel =
        targetDirRel.empty() ? platform::pathToUtf8(src.filename())
                             : targetDirRel + "/" + platform::pathToUtf8(src.filename());
    std::string targetAbs = vault::absolutePath(ed.project, targetRel);
    platform::ensureDir(vault::absolutePath(ed.project, targetDirRel), nullptr);
    fs::copy_file(src, platform::fsPath(targetAbs), fs::copy_options::overwrite_existing, ec);
    if (ec)
        ed.setStatus(TR("Konnte Datei nicht kopieren: ") + sourceAbs, true);
    else
        ed.setStatus(TR("Datei hinzugefuegt: ") + targetRel);
}

void drawPreview(Editor& ed, FileManagerState& st) {
    if (st.selected.empty()) {
        ui::textSecondary(TR("Keine Datei ausgewaehlt."));
        return;
    }
    std::string abs = vault::absolutePath(ed.project, st.selected);
    ImGui::TextUnformatted(fileName(st.selected).c_str());
    ui::textSecondary(st.selected.c_str());
    ImGui::Separator();

    if (ImGui::Button(TR("Im Explorer oeffnen"))) platform::openInShell(abs);
    ImGui::SameLine();
    if (ImGui::Button(TR("Umbenennen"))) {
        std::string rel = st.selected;
        dialogs::prompt(ed, TR("Datei umbenennen"), TR("Neuer Name"), fileName(rel),
                        [&ed, rel](const std::string& value) {
                            if (value.empty()) return;
                            std::error_code ec;
                            std::string dir = parentDir(rel);
                            std::string target = dir.empty() ? value : dir + "/" + value;
                            fs::rename(platform::fsPath(vault::absolutePath(ed.project, rel)),
                                       platform::fsPath(vault::absolutePath(ed.project, target)), ec);
                            if (ec)
                                ed.setStatus(TR("Umbenennen fehlgeschlagen."), true);
                            else
                                ed.setStatus(TR("Umbenannt in ") + value);
                        });
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, theme::withAlpha(theme::colors().errorColor, 0.7f));
    if (ImGui::Button(TR("Loeschen"))) {
        std::string rel = st.selected;
        std::string details;
        if (rel.size() > 3 && rel.substr(rel.size() - 3) == ".md")
            details = TR("Achtung: .md-Dateien gehoeren zu Elementen. Loeschen kann Verweise brechen.");
        dialogs::confirm(ed, TR("Datei loeschen"), "\"" + fileName(rel) + TR("\" wirklich loeschen?"), details,
                         [&ed, rel]() {
                             std::error_code ec;
                             fs::remove(platform::fsPath(vault::absolutePath(ed.project, rel)), ec);
                             ed.setStatus(ec ? TR("Loeschen fehlgeschlagen.") : TR("Datei geloescht."), !!ec);
                             state().selected.clear();
                         });
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::TextUnformatted(TR("Mit Element verknuepfen:"));
    ImGui::SetNextItemWidth(220.0f);
    ui::elementCombo(ed, "##linkel", st.linkElementId, true);
    ImGui::SameLine();
    if (ImGui::Button(TR("Verknuepfen")) && !st.linkElementId.empty()) {
        Element* el = ed.project.element(st.linkElementId);
        if (el) {
            ed.pushUndo(TR("Datei verknuepft"));
            std::vector<std::string> list = listFromValue(el->values["attachments"]);
            if (std::find(list.begin(), list.end(), st.selected) == list.end())
                list.push_back(st.selected);
            el->values["attachments"] = listToValue(list);
            if (std::find(el->fieldOrder.begin(), el->fieldOrder.end(), "attachments") ==
                el->fieldOrder.end())
                el->fieldOrder.push_back("attachments");
            ed.markElement(el->id);
            ed.setStatus(TR("Mit ") + el->name + TR(" verknuepft."));
        }
    }

    ImGui::Separator();
    if (TextureCache::isImage(abs)) {
        int w = 0, h = 0;
        ImTextureID tex = textures().get(abs, &w, &h);
        if (tex && w > 0) {
            float avail = ImGui::GetContentRegionAvail().x;
            float scale = std::min(1.0f, avail / static_cast<float>(w));
            ImGui::Image(tex, ImVec2(static_cast<float>(w) * scale, static_cast<float>(h) * scale));
            ui::textSecondary((std::to_string(w) + " x " + std::to_string(h) + " px").c_str());
        } else {
            ui::textSecondary(TR("Bild konnte nicht geladen werden."));
        }
        return;
    }

    if (st.previewLoaded != st.selected) {
        st.previewText.clear();
        platform::readFile(abs, &st.previewText);
        if (st.previewText.size() > 60000) st.previewText.resize(60000);
        st.previewLoaded = st.selected;
    }
    ImGui::TextUnformatted(TR("Rohinhalt (nur Ansicht):"));
    ImGui::InputTextMultiline("##raw", &st.previewText, ImVec2(-1, -1),
                              ImGuiInputTextFlags_ReadOnly);
}

void drawStructured(Editor& ed, FileManagerState& st) {
    for (const Group& g : ed.project.groups) {
        if (!g.parentId.empty()) continue;
        std::function<void(const Group&)> walk = [&](const Group& group) {
            ui::colorDot(ed.project.groupColor(group.id));
            if (ImGui::TreeNodeEx(group.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const Group* child : ed.project.childGroups(group.id)) walk(*child);
                for (const Element* el : ed.project.groupElements(group.id)) {
                    bool selected = st.selected == el->filePath;
                    ImGui::PushID(el->id.c_str());
                    if (ImGui::Selectable((fileName(el->filePath) + "##f").c_str(), selected)) {
                        st.selected = el->filePath;
                        ed.select(SelKind::Element, el->id);
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
        };
        walk(g);
    }

    ImGui::Separator();
    const char* assetDirs[] = {"Assets/Images", "Assets/Documents", TR("Timeline"), "Actions",
                               TR("Connections")};
    for (const char* dir : assetDirs) {
        if (!ImGui::TreeNode(dir)) continue;
        std::error_code ec;
        fs::path abs = platform::fsPath(vault::absolutePath(ed.project, dir));
        if (fs::exists(abs, ec)) {
            for (const auto& entry : fs::directory_iterator(abs, ec)) {
                std::string name = platform::pathToUtf8(entry.path().filename());
                std::string rel = std::string(dir) + "/" + name;
                if (entry.is_directory()) {
                    ui::textSecondary((name + "/").c_str());
                    continue;
                }
                if (ImGui::Selectable(rel.c_str(), st.selected == rel)) st.selected = rel;
            }
        }
        ImGui::TreePop();
    }
}

void drawRaw(Editor& ed, FileManagerState& st) {
    ImGui::TextUnformatted("/");
    ImGui::SameLine();
    ui::textSecondary(st.currentDir.empty() ? TR("(Vault-Wurzel)") : st.currentDir.c_str());
    if (!st.currentDir.empty()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("..")) st.currentDir = parentDir(st.currentDir);
    }
    ImGui::Separator();

    std::error_code ec;
    fs::path dirAbs = platform::fsPath(vault::absolutePath(ed.project, st.currentDir));
    if (!fs::exists(dirAbs, ec)) {
        ui::textSecondary(TR("Ordner existiert nicht."));
        return;
    }
    std::vector<fs::directory_entry> dirs, files;
    for (const auto& entry : fs::directory_iterator(dirAbs, ec)) {
        if (entry.is_directory())
            dirs.push_back(entry);
        else
            files.push_back(entry);
    }
    auto byName = [](const fs::directory_entry& a, const fs::directory_entry& b) {
        return a.path().filename() < b.path().filename();
    };
    std::sort(dirs.begin(), dirs.end(), byName);
    std::sort(files.begin(), files.end(), byName);

    for (const auto& entry : dirs) {
        std::string name = platform::pathToUtf8(entry.path().filename());
        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors().accentColor);
        if (ImGui::Selectable(("[ " + name + " ]").c_str(), false,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                st.currentDir = st.currentDir.empty() ? name : st.currentDir + "/" + name;
        }
        ImGui::PopStyleColor();
    }
    for (const auto& entry : files) {
        std::string name = platform::pathToUtf8(entry.path().filename());
        std::string rel = st.currentDir.empty() ? name : st.currentDir + "/" + name;
        if (ImGui::Selectable(name.c_str(), st.selected == rel)) st.selected = rel;
        if (ImGui::BeginPopupContextItem()) {
            st.selected = rel;
            if (ImGui::MenuItem(TR("Oeffnen")))
                platform::openInShell(vault::absolutePath(ed.project, rel));
            if (ImGui::MenuItem(TR("Verschieben nach..."))) {
                dialogs::prompt(ed, TR("Datei verschieben"), TR("Zielordner (relativ)"), parentDir(rel),
                                [&ed, rel](const std::string& value) {
                                    std::error_code ec2;
                                    std::string target =
                                        value.empty() ? fileName(rel) : value + "/" + fileName(rel);
                                    platform::ensureDir(vault::absolutePath(ed.project, value), nullptr);
                                    fs::rename(platform::fsPath(vault::absolutePath(ed.project, rel)),
                                               platform::fsPath(vault::absolutePath(ed.project, target)),
                                               ec2);
                                    ed.setStatus(ec2 ? TR("Verschieben fehlgeschlagen.")
                                                     : TR("Verschoben nach ") + target,
                                                 !!ec2);
                                });
            }
            ImGui::EndPopup();
        }
    }
    if (dirs.empty() && files.empty()) ui::textSecondary(TR("Ordner ist leer."));
}

}  // namespace

void drawFileManagerWindow(Editor& ed, bool* open) {
    ImGui::SetNextWindowSize(ImVec2(820, 520), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(TWIN("Dateimanager", "files"), open)) {
        ImGui::End();
        return;
    }
    if (!ed.project.loaded) {
        ui::textSecondary(TR("Kein Projekt geoeffnet."));
        ImGui::End();
        return;
    }

    FileManagerState& st = state();

    if (ImGui::RadioButton(TR("Strukturiert"), !st.rawMode)) st.rawMode = false;
    ImGui::SameLine();
    if (ImGui::RadioButton(TR("Rohe Dateien"), st.rawMode)) st.rawMode = true;
    ImGui::SameLine();
    if (ImGui::Button(TR("Datei hochladen..."))) {
        std::vector<std::string> picked = platform::pickFiles(TR("Dateien in den Vault kopieren"));
        std::string target = st.rawMode ? st.currentDir : std::string("Assets/Images");
        for (const std::string& file : picked) copyInto(ed, file, target);
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("Neuer Ordner..."))) {
        std::string base = st.rawMode ? st.currentDir : std::string("Assets");
        dialogs::prompt(ed, TR("Neuer Ordner"), "Name", "", [&ed, base](const std::string& value) {
            if (value.empty()) return;
            std::string rel = base.empty() ? value : base + "/" + value;
            platform::ensureDir(vault::absolutePath(ed.project, rel), nullptr);
            ed.setStatus(TR("Ordner erstellt: ") + rel);
        });
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("Vault oeffnen"))) platform::openInShell(ed.project.vaultPath);

    // files dropped from the explorer land in the current folder
    if (!ed.droppedFiles.empty()) {
        std::string target = st.rawMode ? st.currentDir : std::string("Assets/Images");
        for (const std::string& file : ed.droppedFiles) copyInto(ed, file, target);
        ed.droppedFiles.clear();
    }

    ImGui::Separator();
    float leftWidth = ImGui::GetContentRegionAvail().x * 0.42f;
    ImGui::BeginChild("fm_left", ImVec2(leftWidth, 0), ImGuiChildFlags_Borders);
    if (st.rawMode)
        drawRaw(ed, st);
    else
        drawStructured(ed, st);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("fm_right", ImVec2(0, 0), ImGuiChildFlags_Borders);
    drawPreview(ed, st);
    ImGui::EndChild();

    ImGui::End();
}

}  // namespace se

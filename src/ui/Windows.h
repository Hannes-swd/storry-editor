#pragma once

namespace se {

class Editor;

void drawManuscriptWindow(Editor& ed, bool* open);
void drawTimelineWindow(Editor& ed, bool* open);
void drawGroupManagerWindow(Editor& ed, bool* open);
void drawDetailsWindow(Editor& ed, bool* open);
void drawActionsWindow(Editor& ed, bool* open);
void drawStoryVisualizerWindow(Editor& ed, bool* open);
void drawConnectionsWindow(Editor& ed, bool* open);
void drawFileManagerWindow(Editor& ed, bool* open);
void drawSettingsWindow(Editor& ed, bool* open);

}  // namespace se

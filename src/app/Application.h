// Win32 + DirectX11 host for the ImGui based editor.
#pragma once

#include "ui/Editor.h"

namespace se {

class Application {
public:
    int run();

    Editor& editor() { return editor_; }
    void onFilesDropped(const std::vector<std::string>& files);
    void onResize(unsigned width, unsigned height);

private:
    bool createDevice(void* hwnd);
    void destroyDevice();
    void createRenderTarget();
    void destroyRenderTarget();
    void drawDockspace();

    Editor editor_;
    void* hwnd_ = nullptr;
    unsigned resizeWidth_ = 0;
    unsigned resizeHeight_ = 0;
    bool dockspaceInitialised_ = false;
};

}  // namespace se

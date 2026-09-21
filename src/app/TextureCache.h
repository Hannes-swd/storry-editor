// Lazily uploads images from the vault to D3D11 textures for the file manager.
#pragma once

#include <map>
#include <string>

#include "imgui.h"

struct ID3D11Device;
struct ID3D11ShaderResourceView;

namespace se {

class TextureCache {
public:
    void init(ID3D11Device* device);
    void shutdown();
    // Returns 0 when the file is not a loadable image.
    ImTextureID get(const std::string& absolutePath, int* width, int* height);
    static bool isImage(const std::string& path);

private:
    struct Entry {
        ID3D11ShaderResourceView* srv = nullptr;
        int width = 0;
        int height = 0;
    };
    std::map<std::string, Entry> entries_;
    ID3D11Device* device_ = nullptr;
};

TextureCache& textures();

}  // namespace se

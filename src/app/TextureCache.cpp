#include "app/TextureCache.h"

#include <algorithm>
#include <cctype>

#include <d3d11.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBI_ONLY_TGA
#include <stb/stb_image.h>

#include "app/Platform.h"

namespace se {

TextureCache& textures() {
    static TextureCache cache;
    return cache;
}

void TextureCache::init(ID3D11Device* device) { device_ = device; }

void TextureCache::shutdown() {
    for (auto& kv : entries_) {
        if (kv.second.srv) kv.second.srv->Release();
    }
    entries_.clear();
    device_ = nullptr;
}

bool TextureCache::isImage(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::string ext = path.substr(dot + 1);
    for (char& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "gif" ||
           ext == "tga";
}

ImTextureID TextureCache::get(const std::string& absolutePath, int* width, int* height) {
    auto it = entries_.find(absolutePath);
    if (it != entries_.end()) {
        if (width) *width = it->second.width;
        if (height) *height = it->second.height;
        return reinterpret_cast<ImTextureID>(it->second.srv);
    }
    Entry entry;
    entries_[absolutePath] = entry;  // negative cache until proven otherwise
    if (!device_ || !isImage(absolutePath)) return 0;

    std::string bytes;
    if (!platform::readFile(absolutePath, &bytes) || bytes.empty()) return 0;

    int w = 0, h = 0, channels = 0;
    unsigned char* pixels =
        stbi_load_from_memory(reinterpret_cast<const unsigned char*>(bytes.data()),
                              static_cast<int>(bytes.size()), &w, &h, &channels, 4);
    if (!pixels) return 0;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(w);
    desc.Height = static_cast<UINT>(h);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = pixels;
    data.SysMemPitch = static_cast<UINT>(w * 4);

    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = device_->CreateTexture2D(&desc, &data, &texture);
    stbi_image_free(pixels);
    if (FAILED(hr) || !texture) return 0;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    ID3D11ShaderResourceView* srv = nullptr;
    hr = device_->CreateShaderResourceView(texture, &srvDesc, &srv);
    texture->Release();
    if (FAILED(hr) || !srv) return 0;

    entry.srv = srv;
    entry.width = w;
    entry.height = h;
    entries_[absolutePath] = entry;
    if (width) *width = w;
    if (height) *height = h;
    return reinterpret_cast<ImTextureID>(srv);
}

}  // namespace se

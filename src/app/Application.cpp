#include "app/Application.h"

#include <windows.h>

#include <d3d11.h>
#include <shellapi.h>
#include <tchar.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"

#include "app/Platform.h"
#include "app/TextureCache.h"
#include "app/UiTestDriver.h"
#include "ui/Lang.h"
#include "ui/Theme.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

#ifndef SE_VERSION
#define SE_VERSION "dev"
#endif
#define SE_WIDEN2(x) L##x
#define SE_WIDEN(x) SE_WIDEN2(x)
#define SE_VERSION_W SE_WIDEN(SE_VERSION)

namespace se {
namespace {

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapChain = nullptr;
ID3D11RenderTargetView* g_mainRenderTarget = nullptr;
bool g_swapChainOccluded = false;
Application* g_app = nullptr;

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;

    switch (msg) {
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED) return 0;
            if (g_app) g_app->onResize(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_DROPFILES: {
            HDROP drop = reinterpret_cast<HDROP>(wParam);
            UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            std::vector<std::string> files;
            for (UINT i = 0; i < count; ++i) {
                UINT len = DragQueryFileW(drop, i, nullptr, 0);
                std::wstring buffer(len + 1, L'\0');
                DragQueryFileW(drop, i, buffer.data(), len + 1);
                buffer.resize(len);
                files.push_back(platform::toUtf8(buffer));
            }
            DragFinish(drop);
            if (g_app) g_app->onFilesDropped(files);
            return 0;
        }
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Vier Schnitte einer Familie: normal, fett, kursiv, fett-kursiv. Das
// Manuskript zeigt Hervorhebungen damit als echte Schrift und nicht nur als
// Sternchen. Was auf dem Rechner fehlt, bleibt einfach leer - theme::fontFor()
// faellt dann auf die normale Schrift zurueck.
void loadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    const float size = theme::settings().fontSize;

    struct Family {
        const char* regular;
        const char* bold;
        const char* italic;
        const char* boldItalic;
    };
    const Family families[] = {
        {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/segoeuib.ttf",
         "C:/Windows/Fonts/segoeuii.ttf", "C:/Windows/Fonts/segoeuiz.ttf"},
        {"C:/Windows/Fonts/calibri.ttf", "C:/Windows/Fonts/calibrib.ttf",
         "C:/Windows/Fonts/calibrii.ttf", "C:/Windows/Fonts/calibriz.ttf"},
        {"C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/arialbd.ttf",
         "C:/Windows/Fonts/ariali.ttf", "C:/Windows/Fonts/arialbi.ttf"},
    };

    auto tryLoad = [&](const char* path) -> ImFont* {
        if (!path || GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return nullptr;
        return io.Fonts->AddFontFromFileTTF(path, size);
    };

    theme::Fonts& out = theme::fonts();
    out = theme::Fonts();
    for (const Family& family : families) {
        ImFont* regular = tryLoad(family.regular);
        if (!regular) continue;
        out.regular = regular;
        out.bold = tryLoad(family.bold);
        out.italic = tryLoad(family.italic);
        out.boldItalic = tryLoad(family.boldItalic);
        io.FontDefault = regular;
        return;
    }
    out.regular = io.Fonts->AddFontDefault();
}

}  // namespace

void Application::onFilesDropped(const std::vector<std::string>& files) {
    for (const std::string& f : files) editor_.droppedFiles.push_back(f);
    editor_.setStatus(std::to_string(files.size()) + " Datei(en) empfangen - Dateimanager oeffnen.");
    theme::settings().showFiles = true;
}

void Application::onResize(unsigned width, unsigned height) {
    resizeWidth_ = width;
    resizeHeight_ = height;
}

bool Application::createDevice(void* hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = static_cast<HWND>(hwnd);
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                               levels, 2, D3D11_SDK_VERSION, &sd, &g_swapChain,
                                               &g_device, &level, &g_context);
    if (hr == DXGI_ERROR_UNSUPPORTED) {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, levels, 2,
                                           D3D11_SDK_VERSION, &sd, &g_swapChain, &g_device, &level,
                                           &g_context);
    }
    if (FAILED(hr)) return false;
    createRenderTarget();
    return true;
}

void Application::destroyDevice() {
    destroyRenderTarget();
    if (g_swapChain) {
        g_swapChain->Release();
        g_swapChain = nullptr;
    }
    if (g_context) {
        g_context->Release();
        g_context = nullptr;
    }
    if (g_device) {
        g_device->Release();
        g_device = nullptr;
    }
}

void Application::createRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (!backBuffer) return;
    g_device->CreateRenderTargetView(backBuffer, nullptr, &g_mainRenderTarget);
    backBuffer->Release();
}

void Application::destroyRenderTarget() {
    if (g_mainRenderTarget) {
        g_mainRenderTarget->Release();
        g_mainRenderTarget = nullptr;
    }
}

void Application::drawDockspace() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##DockHost", nullptr, flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("StoryEditorDockspace");
    const bool resetRequested = editor_.resetLayoutRequested;
    if (!dockspaceInitialised_ || resetRequested) {
        dockspaceInitialised_ = true;
        editor_.resetLayoutRequested = false;
        if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr || resetRequested) {
            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

            ImGuiID mainId = dockspaceId;
            ImGuiID leftId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, 0.22f, nullptr, &mainId);
            ImGuiID rightId =
                ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, 0.28f, nullptr, &mainId);
            ImGuiID bottomId =
                ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, 0.42f, nullptr, &mainId);

            ImGui::DockBuilderDockWindow("###groups", leftId);
            ImGui::DockBuilderDockWindow("###details", rightId);
            ImGui::DockBuilderDockWindow("###manuscript", mainId);
            ImGui::DockBuilderDockWindow("###timeline", mainId);
            ImGui::DockBuilderDockWindow("###actions", bottomId);
            ImGui::DockBuilderDockWindow("###story", bottomId);
            ImGui::DockBuilderDockWindow("###connections", bottomId);
            ImGui::DockBuilderDockWindow("###files", bottomId);
            ImGui::DockBuilderDockWindow("###settings", rightId);
            ImGui::DockBuilderFinish(dockspaceId);
        }
    }
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
}

int Application::run() {
    g_app = this;

    // Erststart: Sprache aus der Windows-Anzeigesprache ableiten, danach
    // entscheidet settings.json. / First run: take the language from Windows.
    const LANGID uiLang = ::GetUserDefaultUILanguage();
    theme::settings().language =
        PRIMARYLANGID(uiLang) == LANG_GERMAN ? Language::German : Language::English;
    lang::set(theme::settings().language);

    theme::load();  // the style itself is applied once the ImGui context exists

    WNDCLASSEXW wc = {sizeof(wc),      CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandleW(nullptr),
                      nullptr,         nullptr,    nullptr, nullptr, L"StoryEditorWindow", nullptr};
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Story Editor " SE_VERSION_W, WS_OVERLAPPEDWINDOW, 80, 60, 1600,
                                950, nullptr, nullptr, wc.hInstance, nullptr);
    hwnd_ = hwnd;

    if (!createDevice(hwnd)) {
        destroyDevice();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        MessageBoxW(nullptr, L"DirectX 11 konnte nicht initialisiert werden.", L"Story Editor",
                    MB_ICONERROR);
        return 1;
    }

    // Oberflaechentest: Fenster ausserhalb des Bildschirms, ohne den Fokus zu
    // nehmen - echte Maus und Tastatur erreichen es nicht.
    const bool testRun = uitest::active();
    if (testRun) {
        ::SetWindowPos(hwnd, nullptr, -5000, -5000, 1600, 950, SWP_NOZORDER | SWP_NOACTIVATE);
        ::ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    } else {
        ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    }
    ::UpdateWindow(hwnd);
    ::DragAcceptFiles(hwnd, TRUE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (!testRun) io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // im Test alles in einem Bild
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    // v2: die Fenster haben feste ###-IDs bekommen, alte Layouts passen nicht mehr.
    static std::string iniPath = platform::appConfigDir() + "/imgui_layout_v3.ini";
    io.IniFilename = iniPath.c_str();

    theme::applyImGuiStyle();
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 4.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_device, g_context);
    loadFonts();
    textures().init(g_device);

    editor_.init();

    bool running = true;
    while (running) {
        MSG msg;
        while (::PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;

        if (!testRun && g_swapChainOccluded && g_swapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            ::Sleep(10);
            continue;
        }
        g_swapChainOccluded = false;

        if (resizeWidth_ != 0 && resizeHeight_ != 0) {
            destroyRenderTarget();
            g_swapChain->ResizeBuffers(0, resizeWidth_, resizeHeight_, DXGI_FORMAT_UNKNOWN, 0);
            resizeWidth_ = resizeHeight_ = 0;
            createRenderTarget();
        }

        // Schriftarten, die das Manuskript im letzten Frame angefordert hat
        theme::loadPendingFonts();
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        if (testRun) {
            uitest::beforeNewFrame(editor_);
            if (uitest::finished()) running = false;
        }
        ImGui::NewFrame();

        editor_.newFrame(io.DeltaTime);
        drawDockspace();
        editor_.drawMainMenuBar();
        editor_.drawWindows();
        editor_.handleShortcuts();
        editor_.flushSaves();
        if (editor_.quitRequested) running = false;

        ImGui::Render();
        const ImVec4& bg = theme::colors().backgroundColor;
        const float clear[4] = {bg.x, bg.y, bg.z, 1.0f};
        g_context->OMSetRenderTargets(1, &g_mainRenderTarget, nullptr);
        g_context->ClearRenderTargetView(g_mainRenderTarget, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        if (testRun) uitest::afterRender(g_device, g_context, g_swapChain);
        HRESULT hr = g_swapChain->Present(testRun ? 0 : 1, 0);
        g_swapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    editor_.shutdown();
    textures().shutdown();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    destroyDevice();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    g_app = nullptr;
    return 0;
}

}  // namespace se

#define NOMINMAX
#include <Windows.h>

#include "SimulatorView.hpp"

#include <Log.hpp>

#include <stb_image.h>
#include <imgui.h>
#include "imgui_platform\imgui_impl_win32.h"
#include "imgui_platform\imgui_impl_dx10.h"


SimulatorRenderView::SimulatorRenderView(std::atomic_bool& running)
    : m_running(running)
    , m_flicker(false)
    , m_idleBeam(false)
{
    loadSettings();
}

SimulatorRenderView::~SimulatorRenderView()
{
    saveSettings();
    if (m_imageData) stbi_image_free(m_imageData);
    m_running = false;
    m_frameSyncPoint.close();
    if (m_windowThread.joinable()) m_windowThread.join();
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI SimulatorRenderView::PlotterWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    SimulatorRenderView* pThis = NULL;
    if (msg == WM_CREATE) {
        // special case, store the context
        pThis = static_cast<SimulatorRenderView*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    }

    switch (msg) {
        case WM_SIZE:
            pThis = reinterpret_cast<SimulatorRenderView*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
            if (pThis->m_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
                pThis->cleanupRenderTarget();
                pThis->m_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                pThis->createRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)  // Disable ALT application menu
                return 0;
            break;
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void SimulatorRenderView::WinMainProc()
{
    const int c_windowWidth = 800;
    const int c_windowHeight = 1000;

    std::wstring title = L"Oscilloscope";

    // Create application window
    WNDCLASSEX wc = {sizeof(WNDCLASSEX), CS_CLASSDC, PlotterWndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("SimulatorRenderView"), NULL};
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow(wc.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW, 100, 100, c_windowWidth, c_windowHeight, NULL, NULL, wc.hInstance, this);

    // Initialize Direct3D
    if (FAILED(createDeviceD3D(hWnd))) {
        cleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        m_running = false;
        printf("Failed to create D3D device. %s", GetLastErrorString());
        m_frameSyncPoint.close();
        return;
    }
    m_width = 400;
    m_height = 400;
    if (m_imageData) {
        m_width = m_imageWidth;
        m_height = m_imageHeight;

        D3D10_TEXTURE2D_DESC desc{0};
        desc.Width = m_width;
        desc.Height = m_height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D10_USAGE_DYNAMIC;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;

        HRESULT hr;
        if (FAILED(hr = m_pd3dDevice->CreateTexture2D(&desc, NULL, &m_texture))) {
            printf("Texture creation failed. %s", HResultToString(hr));
            return;
        }

        D3D10_SHADER_RESOURCE_VIEW_DESC rdesc{};
        rdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rdesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
        rdesc.Texture2D = {0, 1};  // D3D10_TEX2D_SRV
        if (FAILED(hr = m_pd3dDevice->CreateShaderResourceView(m_texture, &rdesc, &m_resourceView))) {
            printf("Shared texture creation failed. %s", HResultToString(hr));
            return;
        }
        m_textureId = m_resourceView;

        D3D10_MAPPED_TEXTURE2D mapped;
        if (FAILED(hr = m_texture->Map(0, D3D10_MAP_WRITE_DISCARD, 0, &mapped))) {
            printf("Texture map failed. %s", HResultToString(hr));
            return;
        }
        RGBQUAD* dst = (RGBQUAD*)mapped.pData;
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                // RGBQUAD c = {0x80, 0x80, 00, 0xFF};
                dst[x] = ((RGBQUAD*)m_imageData)[x + y * m_width];
            }
            dst = (RGBQUAD*)((unsigned char*)mapped.pData + y * mapped.RowPitch);
        }
        m_texture->Unmap(0);
    }

    // Show the window
    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX10_Init(m_pd3dDevice);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error
    // and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which
    // ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'misc/fonts/README.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    // IM_ASSERT(font != NULL);


    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT && m_running) {
        // Poll and handle messages (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX10_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
#if 0
        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");  // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");           // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);  // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);             // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color);  // Edit 3 floats representing a color

            if (ImGui::Button("Button"))  // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window) {
            ImGui::Begin("Another Window",
                &show_another_window);  // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me")) show_another_window = false;
            ImGui::End();
        }
#endif
        ImGui::SetNextWindowPos(ImVec2(13, 30), ImGuiCond_FirstUseEver);

        ImGui::SetNextWindowSizeConstraints(
            {100, 100}, {1024, 1024},
            [](ImGuiSizeCallbackData* data) {
                SimulatorRenderView* pThis = (SimulatorRenderView*)data->UserData;
                auto region = ImGui::GetContentRegionAvail();
                float aspect = pThis->m_width / float(pThis->m_height + 30);
                data->DesiredSize.y = data->DesiredSize.x / aspect;
            },
            this);
        ImGui::Begin("Scope", NULL, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::SetWindowSize(ImVec2(588, 794), ImGuiCond_FirstUseEver);

        if (m_textureId) {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::Image(m_textureId, ImGui::GetContentRegionAvail());
            ImGui::SetCursorScreenPos(pos);
        }

        {
            // Notify application thread blocking in WaitSync that it can start submitting
            std::unique_lock<std::mutex> lock(m_mutex);
            m_frameSyncPoint.sync(lock);

            // theoretically it's possible application thread notifies submit before this thread gets to wait
            // but we don't care.
            m_frameSubmitCv.wait_for(lock, std::chrono::milliseconds(20));
        }

        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(458, 704), ImGuiCond_FirstUseEver);
        ImGui::Begin("Settings", NULL);
        ImGui::SetWindowSize(ImVec2(229, 182), ImGuiCond_FirstUseEver);
        ImGui::DragInt("Width", &m_drawWidth);
        ImGui::DragInt("Height", &m_drawHeight);
        ImGui::DragInt("Xoffset", &m_drawXOffset);
        ImGui::DragInt("Yoffset", &m_drawYOffset);
        ImGui::Checkbox("Flicker", &m_flicker);
        ImGui::Checkbox("Idlespot", &m_idleBeam);
        if (ImGui::Button("Revert Settings")) loadSettings();
        ImGui::End();

        // Rendering
        ImGui::Render();
        m_pd3dDevice->OMSetRenderTargets(1, &m_mainRenderTargetView, NULL);
        m_pd3dDevice->ClearRenderTargetView(m_mainRenderTargetView, (float*)&clear_color);
        ImGui_ImplDX10_RenderDrawData(ImGui::GetDrawData());

        m_pSwapChain->Present(1, 0);  // Present with vsync
        // g_pSwapChain->Present(0, 0); // Present without vsync
    }

    ImGui_ImplDX10_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    cleanupDeviceD3D();
    DestroyWindow(hWnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    m_running = false;

    m_frameSyncPoint.close();
}

bool SimulatorRenderView::WaitSync()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_frameSyncPoint.sync(lock);

    // window may have closed while waiting
    return m_running;
}

void SimulatorRenderView::Submit()
{
    if (!m_running) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    // ImGui::GetIO();
    ImVec2 region = ImGui::GetContentRegionAvail();
    // auto region = ImGui::GetWindowSize();
    // auto wpos = ImGui::GetWindowPos();
    ImVec2 wpos = ImGui::GetCursorScreenPos();

    if (m_flicker) {
        // wpos.x = wpos.x + (-1 + GetTickCount() % 3);
        float rel = m_operations.size() / 100.0f;
        rel = std::min(rel, 1.5f);
        wpos.x = wpos.x + (-rel + rel * (GetTickCount() % 3));
        wpos.y = wpos.y + (-rel + rel * (GetTickCount() % 3));
    }
    int width = m_drawWidth;
    int height = m_drawHeight;

    // bright green
    const ImU32 color = ImGui::ColorConvertFloat4ToU32({76 / 255.f, 224 / 255.f, 76 / 255.f, 1.f});

    auto p2p = [&](const AudioRender::Point& p) -> ImVec2 {
        return {p.x * width + wpos.x + region.x / 2 + m_drawXOffset, p.y * height + wpos.y + region.y / 2 + m_drawYOffset};
    };

    for (size_t i = 0; i < m_operations.size(); i++) {
        const GraphicsPrimitive& p = m_operations[i];
        switch (p.type) {
            case GraphicsPrimitive::Type::DRAW_CIRCLE: {
                drawList->AddCircle(p2p(p.p), width * p.r, color, std::lround(p.r * 50.f), 8 * p.intensity);
            } break;
            case GraphicsPrimitive::Type::DRAW_LINE: {
                drawList->AddLine(p2p(p.p), p2p(p.toPoint), color, 8 * p.intensity);
            } break;
            case GraphicsPrimitive::Type::DRAW_SYNC:
                /*ignore*/
                break;
            default:
                // Unknown
                break;
        }
    }
    if (m_idleBeam) {
        // Simulate empty data blocks that stop the beam on the middle
        if (m_operations.size() < 100) {
            float size = std::min(100.f / m_operations.size(), 20.f);
            drawList->AddCircle(p2p({0, 0}), 0.2f, color, 10, size);
        }
    }

    m_frameSubmitCv.notify_one();
}

bool SimulatorRenderView::start()
{
    m_windowThread = std::thread(&SimulatorRenderView::WinMainProc, this);

    return true;
}

bool SimulatorRenderView::loadBackground(const char* image)
{
    FILE* f;
    if (fopen_s(&f, image, "rb") == 0) {
        int channels;
        m_imageData = stbi_load_from_file(f, &m_imageWidth, &m_imageHeight, &channels, 4);
        fclose(f);
        return true;
    }
    return false;
}

static const char* s_iniFile = ".\\view.ini";
static const char* s_posSection = "position";

void SimulatorRenderView::loadSettings()
{
    // Using Windows 3.1 16-bit API from early 90's
    m_drawWidth = GetPrivateProfileIntA(s_posSection, "width", 274, s_iniFile);
    m_drawHeight = GetPrivateProfileIntA(s_posSection, "height", 272, s_iniFile);
    m_drawXOffset = GetPrivateProfileIntA(s_posSection, "xoffset", 4, s_iniFile);
    m_drawYOffset = GetPrivateProfileIntA(s_posSection, "yoffset", -14, s_iniFile);
    m_flicker = GetPrivateProfileIntA(s_posSection, "flicker", 0, s_iniFile);
    m_idleBeam = GetPrivateProfileIntA(s_posSection, "idlebeam", 0, s_iniFile);
}

void SimulatorRenderView::saveSettings()
{
    char buffer[32];
    auto _itos = [&](int x) {
        _itoa_s(x, buffer, 10);
        return buffer;
    };

    WritePrivateProfileStringA(s_posSection, "width", _itos(m_drawWidth), s_iniFile);
    WritePrivateProfileStringA(s_posSection, "height", _itos(m_drawHeight), s_iniFile);
    WritePrivateProfileStringA(s_posSection, "xoffset", _itos(m_drawXOffset), s_iniFile);
    WritePrivateProfileStringA(s_posSection, "yoffset", _itos(m_drawYOffset), s_iniFile);
    WritePrivateProfileStringA(s_posSection, "flicker", _itos(m_flicker), s_iniFile);
    WritePrivateProfileStringA(s_posSection, "idlebeam", _itos(m_idleBeam), s_iniFile);
}

void SimulatorRenderView::createRenderTarget()
{
    SmartPtr<ID3D10Texture2D> pBackBuffer;
    m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    m_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &m_mainRenderTargetView);
}

void SimulatorRenderView::cleanupRenderTarget() { m_mainRenderTargetView = NULL; }

void SimulatorRenderView::cleanupDeviceD3D()
{
    cleanupRenderTarget();
    m_pSwapChain = NULL;
    m_pd3dDevice = NULL;
}

HRESULT SimulatorRenderView::createDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#if _DEBUG
    createDeviceFlags |= D3D10_CREATE_DEVICE_DEBUG;
#endif
    if (D3D10CreateDeviceAndSwapChain(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, D3D10_SDK_VERSION, &sd, &m_pSwapChain, &m_pd3dDevice) != S_OK)
        return E_FAIL;

    createRenderTarget();

    return S_OK;
}

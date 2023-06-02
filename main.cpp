#include "Main.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "imstb_rectpack.h"
#include "imstb_textedit.h"
#include "imstb_truetype.h"

#include "Font.h"

#include <tchar.h>
#include <d3d11.h>
#pragma comment(lib,"d3d11.lib")
#pragma warning( disable : 4996 )

#define MESSAGEBOX_TIELE "My_Ai"
float Version = 1.4;

static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


//释放句柄
static VOID close_handle(HANDLE* info) {
    if (*info) {
        CloseHandle(*info);
        *info = nullptr;
    }
}

//程序重复检测
static BOOL CheckRepeat() {
    SetLastError(0);
    HANDLE repeat_handle = CreateMutexA(NULL, FALSE, "Repeat");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxA(NULL, "已经存在一个程序运行", MESSAGEBOX_TIELE, MB_OK);
        close_handle(&repeat_handle);
        return FALSE;
    }
    return TRUE;
}

//获取管理员权限
static inline bool Load_UAC() {

    BOOL retn;
    HANDLE hToken;
    LUID Luid;

    //获取打开进程的令牌
    retn = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken);
    if (retn != TRUE) {
        MessageBoxA(NULL, "请使用管理员身份运行", MESSAGEBOX_TIELE, MB_OK);
        return false;
    }

    //查找特权值
    retn = LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &Luid);
    if (retn != TRUE) {
        std::cout << "获取Luid失败" << std::endl;
        return false;
    }

    //给TP和TP里的LUID结构体赋值
    TOKEN_PRIVILEGES tp{}; //新特权结构体
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    tp.Privileges[0].Luid = Luid;

    //调整权限
    AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
    if (GetLastError() != ERROR_SUCCESS) {
        MessageBoxA(NULL, "请使用管理员身份运行", MESSAGEBOX_TIELE, MB_OK);
        system("pause");
        return false;
    }
    return true;
}

//加载重复检测
static BOOL CheckHead() {
    if (CheckRepeat())return TRUE;
    return FALSE;
}








int main() {

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("My_Ai"), NULL };
    ::RegisterClassEx(&wc);

    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("My_Ai"), WS_OVERLAPPEDWINDOW, 100, 100, 1, 1, NULL, NULL, wc.hInstance, NULL);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_HIDE);
    ::UpdateWindow(hwnd);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;


    ImGui::StyleColorsDark();


    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    io.IniFilename = nullptr;
    ImFontConfig Font_cfg;
    Font_cfg.FontDataOwnedByAtlas = false;

    //ImFont* Font = io.Fonts->AddFontFromFileTTF("..\\ImGui Tool\\Font.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
    ImFont* Font = io.Fonts->AddFontFromMemoryTTF((void*)Font_data, Font_size, 18.0f, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());
    ImFont* Font_Big = io.Fonts->AddFontFromMemoryTTF((void*)Font_data, Font_size, 24.0f, &Font_cfg, io.Fonts->GetGlyphRangesChineseFull());
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        {
            ImGuiStyle& style = ImGui::GetStyle();
            ImVec4* colors = style.Colors;
            // 修改各个颜色的值，将其设定为白色
            colors[ImGuiCol_Text] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
            colors[ImGuiCol_TextDisabled] = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
            colors[ImGuiCol_WindowBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            colors[ImGuiCol_ChildBg] = ImVec4(0.92f, 0.92f, 0.92f, 1.0f);
            colors[ImGuiCol_PopupBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.98f);
            colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
            colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, .0f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.85f, 0.85f, 0.85f, 0.30f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.85f, 0.85f, 0.85f, 0.40f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.85f, 0.85f, 0.85f, 0.45f);
            colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
            colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.8f, 0.8f, 0.8f, 0.6f);
            colors[ImGuiCol_MenuBarBg] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
            colors[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
            colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 1.0f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.69f, 0.69f, 0.69f, 0.80f);
            colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.69f, 0.69f, 0.69f, 1.0f);
            colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            colors[ImGuiCol_SliderGrab] = ImVec4(0.98f, 0.63f, 0.27f, 0.78f);
            colors[ImGuiCol_SliderGrabActive] = ImVec4(0.98f, 0.63f, 0.27f, 1.00f);

            static bool WinPos = true;//用于初始化窗口位置
            int Screen_Width{ GetSystemMetrics(SM_CXSCREEN) };//获取显示器的宽
            int Screen_Heigth{ GetSystemMetrics(SM_CYSCREEN) };//获取显示器的高

            if (WinPos)//初始化窗口
            {
                ImGui::SetNextWindowPos({ float(Screen_Width - 400) / 2,float(Screen_Heigth - 500) / 2 });
                WinPos = false;//初始化完毕
            }

            bool show_window = true;

            ImGui::Begin(u8"My_Ai", &show_window, 0 | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            if (!show_window)
            {
                exit(0);
            }
            ImGui::SetWindowSize({ 400.0f,500.0f });//设置窗口大小

            ImGui::Text(u8"当前版本号: %.2f", Version);
            ImGui::Separator();
            ImGui::Separator();
            

            ImGui::End();
        }

        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());


        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;




}


bool CreateDeviceD3D(HWND hWnd)
{
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
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
        {
            const RECT* suggested_rect = (RECT*)lParam;
            ::SetWindowPos(hWnd, NULL, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

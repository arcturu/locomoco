#include <windows.h>
#include "utility.h"
#include "renderer.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

HCURSOR g_hCursor{};

LRESULT CALLBACK WindowProcedure(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return 1;
    }

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        // Esc to quit
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }
        break;
    case WM_SETCURSOR:
        if ((lParam & 0xFFFF) == HTCLIENT) {
            SetCursor(g_hCursor);
            return 0;
        }
        break;
    case WM_SIZE:
    {
        int width = lParam & 0xFFFF;
        int height = (lParam >> 16) & 0xFFFF;
        DEBUG_PRINT(L"(%d, %d)\n", width, height);
        break;
    }
    case WM_DPICHANGED:
    {
        // always xdpi == ydpi on windows apps: https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged
        int xdpi = wParam & 0xFFFF;
        int ydpi = (wParam >> 16) & 0xFFFF;
        DEBUG_PRINT(L"dpi: %d\n", xdpi);
        RECT* const prcNewWindow = (RECT*)lParam;
        SetWindowPos(hWnd,
            NULL,
            prcNewWindow->left,
            prcNewWindow->top,
            prcNewWindow->right - prcNewWindow->left,
            prcNewWindow->bottom - prcNewWindow->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        break;
    }
    default:
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
    // Load default cursor
    g_hCursor = LoadCursor(nullptr, IDC_ARROW);
    WNDCLASSEX windowClass{};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.hInstance = hInstance;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.lpszClassName = L"LocomocoWindow";
    if (RegisterClassEx(&windowClass) == 0) {
        MessageBox(nullptr, L"RegisterClassEx failed.", L"Error", MB_OK);
        return -1;
    }

    DWORD windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    int contentWidth = 1920;
    int contentHeight = 1080;
    RECT rect{ 0, 0, contentWidth, contentHeight };
    AdjustWindowRect(&rect, windowStyle, false);
    
    auto hWnd = CreateWindowExW(
        0,
        windowClass.lpszClassName,
        L"Locomoco",
        windowStyle,
        10,
        10,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr);
    ShowWindow(hWnd, SW_SHOWNORMAL);

    lm::Renderer renderer{};
    if (!renderer.InitializeDirectX()) {
        MessageBox(nullptr, L"InitializeDirectX failed.", L"Error", MB_OK);
        return -1;
    }
    if (!renderer.InitializeSwapChain(hWnd, contentWidth, contentHeight)) {
        MessageBox(nullptr, L"InitializeSwapChain failed.", L"Error", MB_OK);
        return -1;
    }
    renderer.InitializeImGui(hWnd);

    while (true) {
        MSG msg{};
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            renderer.BeginFrame();
            ImGui::ShowDemoWindow();
            renderer.EndFrame();
        }
    }

    renderer.Finalize();
    return 0;
}
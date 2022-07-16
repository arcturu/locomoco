#include "app.h"

namespace lm {
bool App::Inititialize(const AppInitializeParams& params) {
    if (!m_renderer.InitializeDirectX()) {
        MessageBox(nullptr, L"InitializeDirectX failed.", L"Error", MB_OK);
        return false;
    }
    if (!m_renderer.InitializeSwapChain(params.hWnd, params.width, params.height)) {
        MessageBox(nullptr, L"InitializeSwapChain failed.", L"Error", MB_OK);
        return false;
    }
    m_renderer.InitializeImGui(params.hWnd);
    return true;
}

void App::Finalize() {
    m_renderer.Finalize();
}

void App::Update() {
    m_frameState = AppFrameState();
    ProcessMessages();
}

void App::Draw() {
    if (!m_renderer.IsInitialized()) {
        return;
    }
    if (m_frameState.isWindowSizeDirty) {
        m_renderer.ResizeSwapChain(m_state.windowWidth, m_state.windowHeight);
    }
    m_renderer.BeginFrame();
    ImGui::ShowDemoWindow();
    m_renderer.EndFrame();
}
}

#pragma once
#include <mutex>
#include <queue>
#include <Windows.h>
#include "renderer.h"

namespace lm {

// A thread safe queue.
template<typename T>
class ConcurrentQueue {
public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.push(value);
    }
    T pop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto value = m_data.front();
        m_data.pop();
        return value;
    }
    bool empty() const {
        return m_data.empty();
    }
private:
    std::queue<T> m_data{};
    std::mutex m_mutex{};
};

class AppState {
public:
    int windowWidth{};
    int windowHeight{};
};

class AppFrameState {
public:
    bool isWindowSizeDirty{};
};

class IAppMessage {
public:
    virtual ~IAppMessage() {}

    // Destructively mutates AppState.
    virtual void UpdateState(AppState& state, AppFrameState& frameState) = 0;
};

class ResizeWindowMessage : public IAppMessage {
public:
    ResizeWindowMessage(int width, int height) : m_width(width), m_height(height) { }

    virtual void UpdateState(AppState& state, AppFrameState& frameState) override {
        state.windowWidth = m_width;
        state.windowHeight = m_height;
        frameState.isWindowSizeDirty = true;
    }
private:
    int m_width{};
    int m_height{};
};

class AppInitializeParams {
public:
    HWND hWnd{}; // hWnd for main window
    int width; // width of main window
    int height; // height of main window
};

class App {
public:
    // Must be called just once.
    bool Inititialize(const AppInitializeParams& params);

    // Must be called just once after Initialize().
    void Finalize();

    // Must be called from main thread.
    // Processes CPU related tasks.
    void Update();

    // Must be called from main thread.
    // Processes GPU related tasks.
    void Draw();

    // Pushes message to update app state.
    // Don't delete the argument because the lifetime of the argument is managed by App. (Ownership moves to the App.)
    void PushMessage(IAppMessage* pMessage) {
        m_messageQueue.push(pMessage);
    }
private:
    ConcurrentQueue<IAppMessage*> m_messageQueue{};
    AppState m_state{}; // stable over frames.
    AppFrameState m_frameState{}; // cleared every frame.
    Renderer m_renderer{};

    void ProcessMessages() {
        while (!m_messageQueue.empty()) {
            auto* pMessage = m_messageQueue.pop();
            pMessage->UpdateState(m_state, m_frameState);
            delete pMessage;
        }
    }
};
}
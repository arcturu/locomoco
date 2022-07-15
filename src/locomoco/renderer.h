#pragma once
#include <tuple>
#include <Windows.h>
#include <comdef.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "utility.h"

#define MAKE_SMART_COM_PTR(_a) _COM_SMARTPTR_TYPEDEF(_a, __uuidof(_a))

MAKE_SMART_COM_PTR(ID3D12Device5);
MAKE_SMART_COM_PTR(ID3D12GraphicsCommandList4);
MAKE_SMART_COM_PTR(ID3D12CommandQueue);
MAKE_SMART_COM_PTR(IDXGISwapChain3);
MAKE_SMART_COM_PTR(IDXGIFactory4);
MAKE_SMART_COM_PTR(IDXGIAdapter1);
MAKE_SMART_COM_PTR(ID3D12Fence);
MAKE_SMART_COM_PTR(ID3D12CommandAllocator);
MAKE_SMART_COM_PTR(ID3D12Resource);
MAKE_SMART_COM_PTR(ID3D12DescriptorHeap);
MAKE_SMART_COM_PTR(ID3D12Debug);
MAKE_SMART_COM_PTR(ID3D12StateObject);
MAKE_SMART_COM_PTR(ID3D12RootSignature);
MAKE_SMART_COM_PTR(ID3DBlob);

namespace lm {

class Renderer {
public:
    bool InitializeDirectX() {
        if (!Utility::SuccessOrLog(CreateDXGIFactory1(IID_PPV_ARGS(&m_pFactory)))) {
            m_pFactory = nullptr;
            return false;
        }

        m_pDevice = CreateDevice(m_pFactory);
        if (m_pDevice == nullptr) {
            return false;
        }

        m_pQueue = CreateCommandQueue(m_pDevice);

        if (!Utility::SuccessOrLog(m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence)))) {
            return false;
        }
        m_pFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        return true;
    }

    // Initializes swap chain and objects that are associated to the window.
    // Call FinalizeSwapChain before re-initializing swap chain (e.g. when the window is resized).
    bool InitializeSwapChain(HWND hWnd, int width, int height) {
        assert(m_pFactory != nullptr);
        assert(m_pDevice != nullptr);

        m_swapChainWidth = width;
        m_swapChainHeight = height;
        m_pSwapChain = CreateWindowSwapChain(m_pFactory, hWnd, width, height);

        // レンダーターゲットビューのデスクリプタ用ヒープを作成
        m_pRtvDescHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, SwapChainCount, false);

        // per-frame のオブジェクトを初期化
        for (int i = 0; i < SwapChainCount; i++) {
            SUCCESS_OR_RETURN_FALSE(m_pDevice->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_FrameObjects[i].pCommandAllocator)));
            SUCCESS_OR_RETURN_FALSE(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_FrameObjects[i].pSwapChainBuffer)));
            m_FrameObjects[i].hRenderTargetView
                = CreateRenderTargetView(m_FrameObjects[i].pSwapChainBuffer, m_pRtvDescHeap, i);
        }

        // コマンドリスト作成
        // これ m_DFrameObjects[0] だけでいいの？ -> 他のを使うときは Reset で渡しているから OK
        SUCCESS_OR_RETURN_FALSE(m_pDevice->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_FrameObjects[0].pCommandAllocator,
            nullptr,
            IID_PPV_ARGS(&m_pCommandList)));

        return true;
    }

    // TODO
    void FinalizeSwapChain() {
        assert(false);
    }

    void InitializeImGui(HWND hWnd) {
        assert(m_pDevice != nullptr);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();

        m_pImGuiDescHeap
            = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

        ImGui_ImplWin32_Init(hWnd);
        ImGui_ImplDX12_Init(
            m_pDevice,
            SwapChainCount,
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            m_pImGuiDescHeap,
            m_pImGuiDescHeap->GetCPUDescriptorHandleForHeapStart(),
            m_pImGuiDescHeap->GetGPUDescriptorHandleForHeapStart());
    }

    void BeginFrame() {
        ImGui_ImplWin32_NewFrame();
        ImGui_ImplDX12_NewFrame();
        ImGui::NewFrame();

        UINT swapChainIndex = m_pSwapChain->GetCurrentBackBufferIndex();
        ResourceBarrier(
            m_pCommandList,
            m_FrameObjects[swapChainIndex].pSwapChainBuffer,
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        float color[] = {0.0f, 0.0f, 0.0f, 1.0f};
        RECT rect{};
        rect.left = 0;
        rect.top = 0;
        rect.right = m_swapChainWidth;
        rect.bottom = m_swapChainHeight;
        m_pCommandList->ClearRenderTargetView(m_FrameObjects[swapChainIndex].hRenderTargetView, color, 1, &rect);
    }

    void EndFrame() {
        UINT swapChainIndex = m_pSwapChain->GetCurrentBackBufferIndex();

        // ImGui 描画
        ImGui::EndFrame();
        ImGui::Render();
        m_pCommandList->OMSetRenderTargets(1, &m_FrameObjects[swapChainIndex].hRenderTargetView, false, nullptr);
        ID3D12DescriptorHeap* imguiHeaps[] = { m_pImGuiDescHeap };
        m_pCommandList->SetDescriptorHeaps(1, imguiHeaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_pCommandList);
        ResourceBarrier(
            m_pCommandList,
            m_FrameObjects[swapChainIndex].pSwapChainBuffer,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);

        // サブミット
        SubmitCommandList();
        m_pSwapChain->Present(0, 0);

        // 次フレームのコマンドリストを用意する
        UINT nextSwapChainIndex = m_pSwapChain->GetCurrentBackBufferIndex();
        // このフレームのコマンドリストの実行が終わるのを待つ（ダブルバッファリングしていない TLAS
        // を動的に書き換えているので）
        // TODO: TLAS をダブルバッファリング？
        m_pFence->SetEventOnCompletion(m_FenceValue, m_pFenceEvent);
        WaitForSingleObject(m_pFenceEvent, INFINITE);

        m_FrameObjects[nextSwapChainIndex].pCommandAllocator->Reset();
        m_pCommandList->Reset(m_FrameObjects[nextSwapChainIndex].pCommandAllocator, nullptr);
    }

    void Finalize()
    {
        // 現在実行されているコマンドの終了を待つ
        m_FenceValue++;
        m_pQueue->Signal(m_pFence, m_FenceValue);
        m_pFence->SetEventOnCompletion(m_FenceValue, m_pFenceEvent);
        WaitForSingleObject(m_pFenceEvent, INFINITE);

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
private:
    static const size_t SwapChainCount = 2;

    // スワップチェーンの一枚ごとにセットされるバッファー
    class FrameObject {
    public:
        ID3D12CommandAllocatorPtr pCommandAllocator{};
        ID3D12ResourcePtr pSwapChainBuffer{};
        D3D12_CPU_DESCRIPTOR_HANDLE hRenderTargetView{};
    };
    FrameObject m_FrameObjects[SwapChainCount]{};

    int m_swapChainWidth{};
    int m_swapChainHeight{};
    IDXGIFactory4Ptr m_pFactory{};
    ID3D12Device5Ptr m_pDevice{};
    ID3D12CommandQueuePtr m_pQueue{};
    IDXGISwapChain3Ptr m_pSwapChain{};
    ID3D12DescriptorHeapPtr m_pRtvDescHeap{}; // render target view
    ID3D12GraphicsCommandList4Ptr m_pCommandList{};
    ID3D12FencePtr m_pFence{};
    HANDLE m_pFenceEvent{};
    UINT64 m_FenceValue{};

    ID3D12DescriptorHeapPtr m_pImGuiDescHeap{};


    void EnableDebugLayer()
    {
#ifdef _DEBUG
        ID3D12DebugPtr pDebug{};
        SUCCESS_OR_RETURN(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug)));
        pDebug->EnableDebugLayer();
#endif
    }

    ID3D12Device5Ptr CreateDevice(IDXGIFactory4Ptr pFactory)
    {
        // Enumerate all adapters.
        for (auto [i, pAdapter] = std::tuple{ 0, IDXGIAdapter1Ptr { nullptr } };
            pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND;
            i++) {
            DXGI_ADAPTER_DESC1 desc;
            pAdapter->GetDesc1(&desc);

            // Except for software adapters
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                continue;
            }

            EnableDebugLayer();

            ID3D12Device5Ptr pDevice;
            if (!Utility::SuccessOrLog(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&pDevice)))) {
                continue;
            }

            // Check if this adapter supports raytracing.
            /*
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5;
            if (!Utility::SuccessOrLog(
                pDevice->CheckFeatureSupport(
                    D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5)))
                || options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
                continue;
            }
            */
            return pDevice;
        }
        return nullptr;
    }

    ID3D12DescriptorHeapPtr CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT count, bool shaderVisible)
    {
        assert(m_pDevice != nullptr);

        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = type;
        desc.NumDescriptors = count;
        desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        ID3D12DescriptorHeapPtr pDescriptorHeap{};
        if (Utility::SuccessOrLog(m_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pDescriptorHeap)))) {
            return pDescriptorHeap;
        }
        return nullptr;
    }

    ID3D12CommandQueuePtr CreateCommandQueue(ID3D12Device5Ptr pDevice)
    {
        ID3D12CommandQueuePtr pQueue;
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (Utility::SuccessOrLog(pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&pQueue)))) {
            return pQueue;
        }
        return nullptr;
    }

    IDXGISwapChain3Ptr CreateWindowSwapChain(IDXGIFactory4Ptr pFactory, HWND hWnd, UINT width, UINT height)
    {
        assert(m_pQueue != nullptr);

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.BufferCount = SwapChainCount;
        desc.Width = width;
        desc.Height = height;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.SwapEffect
            = DXGI_SWAP_EFFECT_FLIP_DISCARD; // https://docs.microsoft.com/en-us/windows/win32/api/dxgi/ne-dxgi-dxgi_swap_effect
        desc.SampleDesc.Count = 1;

        // CreateSwapChainForHwnd は SwapChain1 しか受け取れないので、まず SwapChain1 を作ってから
        // SwapChain3 に変換する
        MAKE_SMART_COM_PTR(IDXGISwapChain1);
        IDXGISwapChain1Ptr pSwapChain1;
        IDXGISwapChain3Ptr pSwapChain3;
        SUCCESS_OR_RETURN_NULL(pFactory->CreateSwapChainForHwnd(m_pQueue, hWnd, &desc, nullptr, nullptr, &pSwapChain1));
        SUCCESS_OR_RETURN_NULL(pSwapChain1->QueryInterface(IID_PPV_ARGS(&pSwapChain3)));
        return pSwapChain3;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE CreateRenderTargetView(
        ID3D12ResourcePtr pResource, ID3D12DescriptorHeapPtr pDescriptorHeap, UINT index)
    {
        assert(m_pDevice != nullptr);

        D3D12_RENDER_TARGET_VIEW_DESC desc{};
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc.Texture2D.MipSlice = 0;

        D3D12_CPU_DESCRIPTOR_HANDLE handle = pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += index * m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_pDevice->CreateRenderTargetView(pResource, &desc, handle);
        return handle;
    }

    void ResourceBarrier(
        ID3D12GraphicsCommandList4Ptr pCommandList,
        ID3D12ResourcePtr pResource,
        D3D12_RESOURCE_STATES stateBefore,
        D3D12_RESOURCE_STATES stateAfter)
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = pResource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = stateBefore;
        barrier.Transition.StateAfter = stateAfter;
        pCommandList->ResourceBarrier(1, &barrier);
    }

    void SubmitCommandList()
    {
        assert(m_pQueue != nullptr);
        assert(m_pCommandList != nullptr);

        m_pCommandList->Close();
        ID3D12CommandList* pCommandListInterface = m_pCommandList.GetInterfacePtr();
        m_pQueue->ExecuteCommandLists(1, &pCommandListInterface);
        m_FenceValue++;
        m_pQueue->Signal(m_pFence, m_FenceValue);
    }
};

}
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// This exports D3D12SDKVersion and D3D12SDKPath.
#include <d3d12agility.hpp>

#include <directx/d3d12.h>
#include <directx/dxcore.h>
#include <directx/d3dx12.h>
#include <dxguids/dxguids.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <comdef.h>
#include <d3dcompiler.h>

#include <wrl.h>
using namespace Microsoft::WRL;

// Based on https://gist.github.com/stefalie/8a705b7350b74b233087f12b5713e452

#define ARRAY_COUNT(Array) (sizeof(Array)/sizeof((Array)[0]))

#define BACKBUFFER_COUNT 3

static bool DoneRunning;

LRESULT CALLBACK WindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    switch (Message)
    {
    case WM_CLOSE:
    case WM_QUIT:
    {
        DoneRunning = true;
    } break;

    default:
    {
        Result = DefWindowProc(Window, Message, WParam, LParam);
    } break;
    }
    return Result;
}

int main(int argc, char* argv[])
{
    //
    //
    // Win32 Init
    HINSTANCE instance = ::GetModuleHandle(NULL);

    WNDCLASSA WindowClass = {};
    WindowClass.lpfnWndProc = WindowCallback;
    WindowClass.hInstance = instance;
    WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
    WindowClass.lpszClassName = "MinimalD3DWindowClass";
    RegisterClassA(&WindowClass);

    HWND Window = CreateWindowExA(0, WindowClass.lpszClassName,
        "Minimal D3D12",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        0, 0, instance, 0);

    //
    //
    // D3D12 Init

    ID3D12Device10* Device = 0;
    D3D12CreateDevice(0, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&Device));

    IDXGIFactory2* Factory2 = 0;
    CreateDXGIFactory2(0, IID_PPV_ARGS(&Factory2));

    ID3D12CommandQueue* CommandQueue = 0;
    D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = {};
    CommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    CommandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    CommandQueueDesc.NodeMask = 0;
    Device->CreateCommandQueue(&CommandQueueDesc, IID_PPV_ARGS(&CommandQueue));

    IDXGISwapChain1* SwapChain1 = 0;
    DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {};
    SwapChainDesc.Width = 0; // uses window width
    SwapChainDesc.Height = 0; // uses window height
    SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDesc.Stereo = FALSE;
    SwapChainDesc.SampleDesc.Count = 1; // Single sample per pixel
    SwapChainDesc.SampleDesc.Quality = 0;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.BufferCount = BACKBUFFER_COUNT;
    SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    SwapChainDesc.Flags = 0;
    Factory2->CreateSwapChainForHwnd(CommandQueue, // for d3d12, must be command queue instead of device
        Window,
        &SwapChainDesc,
        0, // doesn't support fullscreen
        0,
        &SwapChain1);
    IDXGISwapChain3* SwapChain = 0;
    SwapChain1->QueryInterface(IID_PPV_ARGS(&SwapChain));
    UINT BackbufferIndex = SwapChain->GetCurrentBackBufferIndex();

    ID3D12CommandAllocator* CommandAllocator = 0;
    Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocator));

    ID3D12GraphicsCommandList7* CommandList = 0;
    Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        CommandAllocator, 0,
        IID_PPV_ARGS(&CommandList));
    CommandList->Close();

    //NOTE(chen): resources need descriptors as their handles
    //            descriptor heap is a space for placing descriptors
    ID3D12DescriptorHeap* DescriptorHeap = 0;
    D3D12_DESCRIPTOR_HEAP_DESC DescriptorHeapDesc = {};
    DescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    DescriptorHeapDesc.NumDescriptors = BACKBUFFER_COUNT;
    DescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    DescriptorHeapDesc.NodeMask = 0;
    Device->CreateDescriptorHeap(&DescriptorHeapDesc, IID_PPV_ARGS(&DescriptorHeap));

    ID3D12Resource* Backbuffers[BACKBUFFER_COUNT] = {};
    D3D12_CPU_DESCRIPTOR_HANDLE BackbufferDescriptors[BACKBUFFER_COUNT] = {};
    UINT RTVDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (int BufferIndex = 0; BufferIndex < BACKBUFFER_COUNT; ++BufferIndex)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE BufferRTVDescriptor = DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        BufferRTVDescriptor.ptr += BufferIndex * RTVDescriptorSize;

        ID3D12Resource* Backbuffer = 0;
        SwapChain->GetBuffer(BufferIndex, IID_PPV_ARGS(&Backbuffer));
        Device->CreateRenderTargetView(Backbuffer, 0, BufferRTVDescriptor);

        Backbuffers[BufferIndex] = Backbuffer;
        BackbufferDescriptors[BufferIndex] = BufferRTVDescriptor;
    }

    ID3D12RootSignature* RootSignature = 0;
    D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
    RootSignatureDesc.NumParameters = 0;
    RootSignatureDesc.pParameters = 0;
    RootSignatureDesc.NumStaticSamplers = 0;
    RootSignatureDesc.pStaticSamplers = 0;
    RootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ID3DBlob* Signature = 0;
    D3D12SerializeRootSignature(&RootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1_0,
        &Signature, 0);
    Device->CreateRootSignature(0, Signature->GetBufferPointer(),
        Signature->GetBufferSize(),
        IID_PPV_ARGS(&RootSignature));

    ComPtr<IDxcLibrary> library;
    ::DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));

    // Read the blobs.
    ComPtr<IDxcBlobEncoding> VSCodeBlob;
    library->CreateBlobFromFile(L"basic_vs.dxi", CP_ACP, &VSCodeBlob);

    ComPtr<IDxcBlobEncoding> PSCodeBlob;
    library->CreateBlobFromFile(L"basic_fs.dxi", CP_ACP, &PSCodeBlob);

    D3D12_INPUT_ELEMENT_DESC InputElements[] = {
        {"Position", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"Color", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    ID3D12PipelineState* PSO = 0;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};
    PSODesc.pRootSignature = RootSignature;
    PSODesc.VS = { VSCodeBlob->GetBufferPointer(), VSCodeBlob->GetBufferSize() };
    PSODesc.PS = { PSCodeBlob->GetBufferPointer(), PSCodeBlob->GetBufferSize() };
    PSODesc.BlendState.AlphaToCoverageEnable = FALSE;
    PSODesc.BlendState.IndependentBlendEnable = FALSE;
    for (int I = 0; I < ARRAY_COUNT(PSODesc.BlendState.RenderTarget); ++I)
    {
        PSODesc.BlendState.RenderTarget[I].BlendEnable = FALSE;
        PSODesc.BlendState.RenderTarget[I].LogicOpEnable = FALSE;
        PSODesc.BlendState.RenderTarget[I].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    PSODesc.SampleMask = UINT_MAX;
    PSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    PSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    PSODesc.RasterizerState.FrontCounterClockwise = TRUE;
    PSODesc.DepthStencilState.DepthEnable = FALSE;
    PSODesc.DepthStencilState.StencilEnable = FALSE;
    PSODesc.InputLayout.pInputElementDescs = InputElements;
    PSODesc.InputLayout.NumElements = ARRAY_COUNT(InputElements);
    PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    PSODesc.NumRenderTargets = 1;
    PSODesc.RTVFormats[0] = SwapChainDesc.Format;
    PSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    PSODesc.SampleDesc.Count = 1; // one sample per pixel
    PSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    Device->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&PSO));

    float Vertices[] = {
        // position    color
        0.0f, 0.7f,    1.0f, 0.0f, 0.0f,
        -0.4f, -0.5f,  0.0f, 1.0f, 0.0f,
        0.4f, -0.5f,   0.0f, 0.0f, 1.0f,
    };

    ID3D12Resource* VertexBuffer = 0;
    {
        D3D12_HEAP_PROPERTIES UploadHeapProperties = {};
        UploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
        UploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        UploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        D3D12_RESOURCE_DESC ResourceDesc = {};
        ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        ResourceDesc.Alignment = 0;
        ResourceDesc.Width = sizeof(Vertices);
        ResourceDesc.Height = 1;
        ResourceDesc.DepthOrArraySize = 1;
        ResourceDesc.MipLevels = 1;
        ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        ResourceDesc.SampleDesc.Count = 1;
        ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        Device->CreateCommittedResource(&UploadHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &ResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            0,
            IID_PPV_ARGS(&VertexBuffer));
        D3D12_RANGE ReadRange = {};
        void* MappedAddress = 0;
        VertexBuffer->Map(0, &ReadRange, &MappedAddress);
        memcpy(MappedAddress, Vertices, sizeof(Vertices));
        VertexBuffer->Unmap(0, 0);
    }

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {};
    VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
    VertexBufferView.SizeInBytes = sizeof(Vertices);
    VertexBufferView.StrideInBytes = 5 * sizeof(float);

    // sync objects
    UINT64 FenceValue = 0;
    ID3D12Fence* Fence = 0;
    Device->CreateFence(FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence));
    HANDLE FenceEvent = CreateEventA(0, 0, FALSE, 0);

    //
    //
    // Render Loop

    while (!DoneRunning)
    {
        MSG Message;
        if (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }

        CommandAllocator->Reset();
        CommandList->Reset(CommandAllocator, 0);

        ID3D12Resource* Backbuffer = Backbuffers[BackbufferIndex];
        D3D12_CPU_DESCRIPTOR_HANDLE BackbufferDescriptor = BackbufferDescriptors[BackbufferIndex];

        // Begin render pass.
        float ClearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
        CD3DX12_CLEAR_VALUE clearValue{ DXGI_FORMAT_R8G8B8A8_UNORM, ClearColor };

        D3D12_RENDER_PASS_BEGINNING_ACCESS beginAccess = D3D12_RENDER_PASS_BEGINNING_ACCESS{ D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR, { clearValue } };
        D3D12_RENDER_PASS_ENDING_ACCESS endAccess = D3D12_RENDER_PASS_ENDING_ACCESS{ D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE, { } };
        D3D12_RENDER_PASS_RENDER_TARGET_DESC renderTargetDesc{ BackbufferDescriptor, beginAccess, endAccess };
        CommandList->BeginRenderPass(1, &renderTargetDesc, nullptr, D3D12_RENDER_PASS_FLAG_NONE);

        // Continue actual rendering.
        D3D12_VIEWPORT Viewport = {};
        Viewport.Width = (float)Backbuffer->GetDesc().Width;
        Viewport.Height = (float)Backbuffer->GetDesc().Height;
        Viewport.MinDepth = 0.0f;
        Viewport.MaxDepth = 1.0f;
        D3D12_RECT ScissorRect = {};
        ScissorRect.right = (LONG)Backbuffer->GetDesc().Width;
        ScissorRect.bottom = (LONG)Backbuffer->GetDesc().Height;

        CommandList->SetPipelineState(PSO);
        CommandList->SetGraphicsRootSignature(RootSignature);
        CommandList->RSSetViewports(1, &Viewport);
        CommandList->RSSetScissorRects(1, &ScissorRect);

        D3D12_RESOURCE_BARRIER RenderBarrier = {};
        RenderBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        RenderBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        RenderBarrier.Transition.pResource = Backbuffer;
        RenderBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        RenderBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        CommandList->ResourceBarrier(1, &RenderBarrier);

        //CommandList->OMSetRenderTargets(1, &BackbufferDescriptor, 0, 0);
        //float ClearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
        //CommandList->ClearRenderTargetView(BackbufferDescriptor, ClearColor, 0, 0);
        CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
        CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        CommandList->DrawInstanced(3, 1, 0, 0);

        D3D12_RESOURCE_BARRIER PresentBarrier = {};
        PresentBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        PresentBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        PresentBarrier.Transition.pResource = Backbuffer;
        PresentBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        PresentBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

        // End render pass.
        CommandList->EndRenderPass();

        CommandList->ResourceBarrier(1, &PresentBarrier);
        CommandList->Close();

        ID3D12CommandList* CommandLists[] = { CommandList };
        CommandQueue->ExecuteCommandLists(1, CommandLists);

        SwapChain->Present(1, 0);

        //NOTE(chen): waiting until all the commands are executed,
        //            effectively doing single buffering. It's inefficient
        //            but simple and easy to understand.
        FenceValue += 1;
        CommandQueue->Signal(Fence, FenceValue);
        if (Fence->GetCompletedValue() < FenceValue)
        {
            Fence->SetEventOnCompletion(FenceValue, FenceEvent);
            WaitForSingleObject(FenceEvent, INFINITE);
        }
        BackbufferIndex = SwapChain->GetCurrentBackBufferIndex();

        Sleep(2);
    }

    return 0;
}
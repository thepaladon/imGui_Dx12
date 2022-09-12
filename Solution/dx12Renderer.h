#pragma once
// Get .h files for initialization of dx12
#include "../Common/d3dApp.h"
#include "../Common/UploadBuffer.h"
#include "objectConstf4x4.h"
#include "vertex1.h"

#include "../Common/ImGuiHandler.h"

using Microsoft::WRL::ComPtr;

using namespace DirectX;
using namespace DirectX::PackedVector;

class dx12Renderer : public D3DApp
{
public:
    dx12Renderer(HINSTANCE hInstance);
    dx12Renderer(const dx12Renderer& rhs) = delete;
    dx12Renderer& operator=(const dx12Renderer& rhs) = delete;
    ~dx12Renderer();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    void ImGuiDraw();
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildBoxGeometry();
    void BuildPSO();

    void EnableDebugLayer()
    {
#if defined(_DEBUG)
        // Always enable the debug layer before doing anything DX12 related
        // so all possible errors generated while creating DX12 objects
        // are caught by the debug layer.
        ComPtr<ID3D12Debug> debugInterface;
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
        debugInterface->EnableDebugLayer();
#endif
    }

private:
    // Stores information about constants, passed to shaders
    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    //Constant buffers heap
    ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

    //upload buufer, used to pass cbs (constant buffers) to shaders
    std::unique_ptr<UploadBuffer<objectConstf4x4>> mObjectCB = nullptr;

    //Mesh geometry is a main struct for storing data about models. TODO: now is a part of d3dUtil, probably need to move to a separate class
    // TODO: create a level class, which stores meshes from all the level
    std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;


    // Shaders from HLSL files are converted into Byte code and stored as values
    //mvs - vertex shader; mps - pixel shader; 
    ComPtr<ID3DBlob> mvsByteCode = nullptr;
    ComPtr<ID3DBlob> mpsByteCode = nullptr;

    //mInputLayout - describes not constant values (vertex parameteres like color and position) to shaders
    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    //Unifying part of pipeline, contains information about its parts and settings
    ComPtr<ID3D12PipelineState> mPSO = nullptr;

    // Dear ImGui
    ImGuiHandler imguiHandler;

    //Matricies to transfer verticies from model to camera space 
    // TODO: Camera class, containing this information
    XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();
    //camera settings
    float mTheta = 1.5f * XM_PI;
    float mPhi = XM_PIDIV4;
    float mRadius = 7.5;
    // TODO: CUT input (used to track camera rotation)
    POINT mLastMousePos;
};

#include "dx12Renderer.h"

dx12Renderer::dx12Renderer(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

dx12Renderer::~dx12Renderer()
{
}

bool dx12Renderer::Initialize()
{
   
    //error handling for d3dApp
    if (!D3DApp::Initialize())
        return false;

    EnableDebugLayer();

    // Reset the command list to prepare for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    BuildDescriptorHeaps();
    BuildConstantBuffers();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildBoxGeometry();
    BuildPSO();

    HWND* trash = reinterpret_cast<HWND*>(0xdeadbeefa5541321);

    imguiHandler.SetHwnd(&mhMainWnd);
    imguiHandler.SetHwnd(trash);
    imguiHandler.GetData(md3dDevice.Get(), 2, DXGI_FORMAT_R8G8B8A8_UNORM, mCbvHeap.Get());
    if(!imguiHandler.InitializeImGui())
    {
        MessageBox(0, L"ImGui Fucked up fam.", 0, 0);
    }

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}

void dx12Renderer::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::M_Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void dx12Renderer::Update(const GameTimer& gt)
{
    //For now updates camera parameters, later will be used for backend

    imguiHandler.StartNewFrame();

    
    //TODO: MOVE TO CAMERA CLASS
    // Convert Spherical to Cartesian coordinates.
    float x = mRadius * sinf(mPhi) * cosf(mTheta);
    float z = mRadius * sinf(mPhi) * sinf(mTheta);
    float y = mRadius * cosf(mPhi);

    // Build the view matrix.
    XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);

    XMMATRIX world = XMLoadFloat4x4(&mWorld);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);
    XMMATRIX worldViewProj = world * view * proj;

    // Update the constant buffer with the latest worldViewProj matrix.
    objectConstf4x4 objConstants;
    XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
    mObjectCB->CopyData(0, objConstants);
}


void dx12Renderer::ImGuiDraw(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmd)
{
    ImGui::ShowDemoWindow();

    ImGui::EndFrame();

    ImGui::Render();

    ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());

    //FlushCommandQueue();
}
void dx12Renderer::Draw(const GameTimer& gt)
{
    //ImGui::ShowDemoWindow();

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(mDirectCmdListAlloc->Reset());
    
    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));
    
    mCommandList->RSSetViewports(1, &mScreenViewport);
    // scissors rectangle is used to cover, for example, UI areas, so the pixels in this area of screen are not rendered (optimization)
    mCommandList->RSSetScissorRects(1, &mScissorRect);
    
    // Indicate a state transition on the resource usage.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    
    // Clear the back buffer and depth buffer.
    //Depth buffer is used for dynamic shadows
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::PaleVioletRed, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    
    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
    
    //Executing commands based on the previously initialized values
    ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
    
    mCommandList->IASetVertexBuffers(0, 1, &mBoxGeo->VertexBufferView());
    mCommandList->IASetIndexBuffer(&mBoxGeo->IndexBufferView());
    
    //Will have to be changed to line list for the PhysX, for meshes triangles
    mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());
    
    //Example of how to do instancing (later)
    //Draws one  instance of submesh "box" of Box mesh, using index array (not only vertex)
    mCommandList->DrawIndexedInstanced(
        mBoxGeo->DrawArgs["box"].IndexCount,
        1, 0, 0, 0);
    
    ImGuiDraw(mCommandList);
    // Indicate a state transition on the resource usage.
     mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
         D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    
    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());
    
    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    
    // swap the back and front buffers
    
    //2 swap buffers (back and front) is enough for our scale
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;
    
    // Wait until frame commands are complete.  This waiting is inefficient and is
    // done for simplicity.  Later we will show how to organize our rendering code
    // so we do not have to wait per frame.
    FlushCommandQueue();
    ThrowIfFailed(mSwapChain->Present(0, 0));


    //barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    //barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    //mCommandList->ResourceBarrier(1, &barrier);
    //ThrowIfFailed(mCommandList->Close());

    //ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    //mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    //mSwapChain->Present(0, 0);


    //ImGui::EndFrame();
}


void dx12Renderer::BuildDescriptorHeaps()
{
    //For now creates 1 descriptor heap for WVP matrix
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
        IID_PPV_ARGS(&mCbvHeap)));
}

void dx12Renderer::BuildConstantBuffers()
{
    //For now Puts the WVP matrix to a cb (constant buffer) 
    //Objects, which stares data about cbs, can stare multile of them
    mObjectCB = std::make_unique<UploadBuffer<objectConstf4x4>>(md3dDevice.Get(), 1, true);

    //It is useful to use structs for all of the cbs, to know the exact size of them
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(objectConstf4x4));

    D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();
    // Offset to the ith object constant buffer in the buffer.
    //TODO: make the systen work with multiple objects
    int boxCBufIndex = 0;
    cbAddress += boxCBufIndex * objCBByteSize;
    //creation of descriptor, which stares date for creation of cbv
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(objectConstf4x4));

    md3dDevice->CreateConstantBufferView(
        &cbvDesc,
        mCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void dx12Renderer::BuildRootSignature()
{
    // Shader programs typically require resources as input (constant buffers,
    // textures, samplers).  The root signature defines the resources the shader
    // programs expect.  If we think of the shader programs as a function, and
    // the input resources as function parameters, then the root signature can be
    // thought of as defining the function signature.

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];

    // Create a single descriptor table of CBVs.
    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
    //Error handling
    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&mRootSignature)));
}

void dx12Renderer::BuildShadersAndInputLayout()
{
    HRESULT hr = S_OK;
    // Gets shader of hlsl format from Shaders foldes, specifies type VC, PC (vertex shader, pixel shader);
    mvsByteCode = d3dUtil::CompileShader(L"../Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
    mpsByteCode = d3dUtil::CompileShader(L"../Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");
    //describe input layout (verticies info)
    //First "/value name in shader", than index (for exampe two textures can have name "TEXTURES", but use 0 and 1 here) 
    //Then, value type (format), input slot, bites from the start (bites offset), slot class(we would probably use
    //D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, until we implement instancing, same about last value
    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

/*
 DXGI_FORMAT_R32_FLOAT // 1D 32-bit float scalar
 DXGI_FORMAT_R32G32_FLOAT // 2D 32-bit float vector
 DXGI_FORMAT_R32G32B32_FLOAT // 3D 32-bit float vector
 DXGI_FORMAT_R32G32B32A32_FLOAT // 4D 32-bit float vector
 DXGI_FORMAT_R8_UINT // 1D 8-bit unsigned integer scalar
 DXGI_FORMAT_R16G16_SINT // 2D 16-bit signed integer vector
 DXGI_FORMAT_R32G32B32_UINT // 3D 32-bit unsigned integer vector
 DXGI_FORMAT_R8G8B8A8_SINT // 4D 8-bit signed integer vector
 DXGI_FORMAT_R8G8B8A8_UINT // 4D 8-bit unsigned integer vector
    */
}

void dx12Renderer::BuildBoxGeometry()
{
    // Creates Box verticies array
    //TODO: GET RID OF THIS METHOD
    std::array<vertex1, 8> vertices =
    {
        vertex1({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
        vertex1({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
        vertex1({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
        vertex1({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
        vertex1({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
        vertex1({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
        vertex1({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
        vertex1({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
    };
    //Creates indicies array
    //having indicies helps to reduce the cize of vertex array, as we do not need to have replicated verticies for each triangle
    std::array<std::uint16_t, 36> indices =
    {
        // front face
        0, 1, 2,
        0, 2, 3,

        // back face
        4, 6, 5,
        4, 7, 6,

        // left face
        4, 5, 1,
        4, 1, 0,

        // right face
        3, 2, 6,
        3, 6, 7,

        // top face
        1, 5, 6,
        1, 6, 2,

        // bottom face
        4, 0, 3,
        4, 3, 7
    };
    //calculates sizes of vertext and index arrays
    const UINT vbByteSize = (UINT)vertices.size() * sizeof(vertex1);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);
    //Mesh geometry creation
    mBoxGeo = std::make_unique<MeshGeometry>();
    mBoxGeo->Name = "boxGeo";

    //setting up mesh with the created vertex and index arrays
    //Supports error handling
    ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU));
    CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
    CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);


    //creation of buffers from arrays
    mBoxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, mBoxGeo->VertexBufferUploader);

    mBoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, mBoxGeo->IndexBufferUploader);

    //mCommandList.Get()->CopyBufferRegion(mBoxGeo.get()->IndexBufferGPU.Get(), 0, mBoxGeo.get()->IndexBufferGPU.Get(), 0, 10000);
    // With multiple arrays of vertex / index data, the single buffer with offsets can be used
    mBoxGeo->VertexByteStride = sizeof(vertex1);
    mBoxGeo->VertexBufferByteSize = vbByteSize;
    mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    mBoxGeo->IndexBufferByteSize = ibByteSize;
    // This is used for submeshes (store multiple submeshes in one mesh => all their data in one buffer)
    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    mBoxGeo->DrawArgs["box"] = submesh;
}

void dx12Renderer::BuildPSO()
{
    //PSO gathers all the required info for the pipeline
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
        mvsByteCode->GetBufferSize()
    };
    psoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
        mpsByteCode->GetBufferSize()
    };

    //We will probably use default settings for the first sprints (similar to default OpenGl settings + culls backfaces)
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;

    //We will need multile PSOs, as one of the settings is the type of primitive to render
    // For PhysX we will need lines
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // More render targets can be needed for example for mirrors ( well, I think so). RT (render target)
    psoDesc.NumRenderTargets = 1;

    //Backbuffer constantly swithches with frontbuffers, so we do not see the pixels filling the screen (we render to back, then sqitch back and front)
    psoDesc.RTVFormats[0] = mBackBufferFormat;

    //Antialliasing - now default, later will need to test the best one for performance / quality
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = mDepthStencilFormat;

    //error handling
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}
//TODO: CUT INPUT
void dx12Renderer::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void dx12Renderer::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void dx12Renderer::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::M_Pi - 0.1f);
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.005 unit in the scene.
        float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
        float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}
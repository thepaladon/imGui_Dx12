#include "ImGuiHandler.h"

#include <d3d12.h>

ImGuiHandler::ImGuiHandler() {}

ImGuiHandler::~ImGuiHandler() {}

bool ImGuiHandler::InitializeImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    //Later when Ian has made Gamepad Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    ImGui::StyleColorsDark();


    if (!ImGui_ImplWin32_Init(hwnd))
    {
        MessageBox(0, L"ImGui Win32 Could not be Initialized.", 0, 0);
        return false;
    }

    if (!ImGui_ImplDX12_Init(device,
        num_frames_in_flight,
        rtv_format,
        cbv_srv_heap,
        cbv_srv_heap->GetCPUDescriptorHandleForHeapStart(),
        cbv_srv_heap->GetGPUDescriptorHandleForHeapStart()))
    {
        MessageBox(0, L"ImGui DX12 Could not be Initialized.", 0, 0);
        return false;
    }
    
    return true;
}

void ImGuiHandler::StartNewFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiHandler::CleanUp()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiHandler::ImGuiRender()
{
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList);
}


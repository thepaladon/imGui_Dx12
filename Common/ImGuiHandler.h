#pragma once

#include "windows.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

class ImGuiHandler
{
public:
	ImGuiHandler();
	~ImGuiHandler();

	void GetData(ID3D12Device* p_device,
	int p_num_frames_in_flight,
	DXGI_FORMAT p_rtv_format,
	ID3D12DescriptorHeap* p_cbv_srv_heap)
	{
		device = p_device;
		num_frames_in_flight = p_num_frames_in_flight;
		rtv_format = p_rtv_format;
		cbv_srv_heap = p_cbv_srv_heap;
	};

	void GetCommandList(ID3D12GraphicsCommandList* p_pd3dCommandList)
	{
		p_pd3dCommandList = g_pd3dCommandList;
	}

	bool InitializeImGui();
	void StartNewFrame();
	void CleanUp();
	void ImGuiRender();
	void SetHwnd(HWND* hwnd);

	ImGuiIO* GetImGuiIO() { return &io; }

protected:
	ImGuiIO io;

	//win32 references
	HWND* hwnd;

	//DX12 references
	ID3D12Device* device;
	ID3D12GraphicsCommandList* g_pd3dCommandList;
	int num_frames_in_flight;
	DXGI_FORMAT rtv_format;
	ID3D12DescriptorHeap* cbv_srv_heap;
	

};


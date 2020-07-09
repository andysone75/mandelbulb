#pragma once

#include "d3dUtil.h"
#include "MathHelper.h"
#include "UploadBuffer.h"

struct PassConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 WorldView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvWorldView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();

	DirectX::XMFLOAT3 CamPos;
	FLOAT AspectRatio;

	DirectX::XMFLOAT3 Color;
	FLOAT Darkness;

	FLOAT FractalPower;
};

struct FrameResource {
	FrameResource(ID3D12Device* device, UINT passCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;

	UINT64 Fence = 0;
};
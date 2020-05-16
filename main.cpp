#include "d3dApp.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "KeyCode.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

struct Vertex
{
	XMFLOAT3 Pos;
};

struct ObjectConstants
{
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 WorldView = MathHelper::Identity4x4();
	XMFLOAT4X4 InvWorldView = MathHelper::Identity4x4();
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();

	XMFLOAT3 CamPos;
	FLOAT AspectRatio;

	FLOAT Power;

	XMFLOAT3 Color;
	FLOAT Darkness;
};

struct Input
{
	float Horizontal = 0.0f;
	float Vertical = 0.0f;
	
	bool GetKey_E = false;
	bool GetKey_Q = false;
	bool GetKey_LShift = false;
};

class RayMarching : public D3DApp
{
public:
	RayMarching(HINSTANCE hInstance);
	RayMarching(const RayMarching& rhs) = delete;
	RayMarching& operator=(const RayMarching& rhs) = delete;
	~RayMarching();

	virtual bool Initialize() override;
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildGeometry();
	void BuildPSO();

private:
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;
	std::unique_ptr<MeshGeometry> mGeometry = nullptr;

	ComPtr<ID3DBlob> mvsByteCode = nullptr;
	ComPtr<ID3DBlob> mpsByteCode = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	ComPtr<ID3D12PipelineState> mPSO = nullptr;

	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	XMVECTOR mPosition = XMVectorSet(3.0f, 0.0f, -3.0f, 1.0f);
	Input mInput;
	float mMoveSpeedHor = 1.0f;
	float mMoveSpeedVert = 1.0f;
	float mMoveSpeedAcceleration = .1f;

	float mTheta = 3.0f * XM_PIDIV4;
	float mPhi = XM_PIDIV2;

	POINT mLastMousePos;

	float fovAngleY = .25f * MathHelper::Pi;

	float mPower = 8.0f;
	float mPowerGrowSpeed = 0.0f;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	try
	{
		RayMarching theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

RayMarching::RayMarching(HINSTANCE hInstance) :
	D3DApp(hInstance)
{
}

RayMarching::~RayMarching()
{
}

bool RayMarching::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	ThrowIfFailed(mCommandList->Reset(mCommandListAlloc.Get(), nullptr));

	BuildConstantBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildGeometry();
	BuildPSO();

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	FlushCommandQueue();

	return true;
}

LRESULT RayMarching::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_KEYDOWN)
	{
		switch (wParam)
		{
		case VK_RIGHT:
			mPowerGrowSpeed += .01f;
			return 0;
		case VK_LEFT:
			mPowerGrowSpeed -= .01f;
			return 0; 

		case KeyCode::W:
			mInput.Vertical = +1.0f;
			return 0;
		case KeyCode::S:
			mInput.Vertical = -1.0f;
			return 0;
		case KeyCode::A:
			mInput.Horizontal = +1.0f;
			return 0;
		case KeyCode::D:
			mInput.Horizontal = -1.0f;
			return 0;

		case KeyCode::E:
			mInput.GetKey_E = true;
			return 0;
		case KeyCode::Q:
			mInput.GetKey_Q = true;
			return 0;

		case KeyCode::LeftShift:
			mInput.GetKey_LShift = true;
			return 0;
		}
	}
	else if (msg == WM_KEYUP)
	{
		switch (wParam)
		{
		case KeyCode::W:
		case KeyCode::S:
			mInput.Vertical = 0.0f;
			return 0;
		case KeyCode::A:
		case KeyCode::D:
			mInput.Horizontal = 0.0f;
			return 0;
		case KeyCode::E:
			mInput.GetKey_E = false;
			return 0;
		case KeyCode::Q:
			mInput.GetKey_Q = false;
			return 0;
		case KeyCode::LeftShift:
			mInput.GetKey_LShift = false;
			return 0;
		}
	}

	return D3DApp::MsgProc(hwnd, msg, wParam, lParam);
}

void RayMarching::OnResize()
{
	D3DApp::OnResize();

	XMMATRIX P = XMMatrixPerspectiveFovLH(.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void RayMarching::Update(const GameTimer& gt)
{
	float x = sinf(mPhi) * cosf(mTheta);
	float z = sinf(mPhi) * sinf(mTheta);
	float y = cosf(mPhi);


	XMVECTOR pos = mPosition;
	XMVECTOR target = XMVectorSet(x, y, z, 0.0f) + mPosition;
	XMVECTOR up = XMVectorSet(.0f, 1.0f, .0f, .0f);
	
	XMVECTOR lForward = XMVector3Normalize(target - mPosition);
	XMVECTOR lRight = XMVector3Normalize(XMVector3Cross(lForward, up));

	mPosition = mPosition + (
		lForward	* mInput.Vertical	* mMoveSpeedHor
		+ lRight	* mInput.Horizontal * mMoveSpeedHor
		+ up		* (mInput.GetKey_E ? +1.0f : mInput.GetKey_Q ? -1.0f : 0.0f) * mMoveSpeedVert
	) * (mInput.GetKey_LShift ? mMoveSpeedAcceleration : 1.0f) * mTimer.DeltaTime();

	pos = mPosition;
	target = XMVectorSet(x, y, z, 0.0f) + mPosition;


	XMMATRIX world = XMLoadFloat4x4(&mWorld);
	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX worldView = world * view;
	XMMATRIX worldViewProj = worldView * proj;

	worldView = XMMatrixTranspose(worldView);
	worldViewProj = XMMatrixTranspose(worldViewProj);


	ObjectConstants objConstants;

	XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
	XMStoreFloat4x4(&objConstants.WorldView, worldView);
	XMStoreFloat4x4(&objConstants.InvWorldView, XMMatrixInverse(&XMMatrixDeterminant(worldView), worldView));
	XMStoreFloat4x4(&objConstants.WorldViewProj, worldViewProj);

	XMStoreFloat3(&objConstants.CamPos, mPosition);
	objConstants.AspectRatio = AspectRatio();

	objConstants.Power = mPower;

	objConstants.Color = XMFLOAT3(1, 0, 1);
	objConstants.Darkness = 150.0f;

	mObjectCB->CopyData(0, objConstants);


	mPower += mPowerGrowSpeed * mTimer.DeltaTime();
	if (mPower < 1.0f) mPower = 1.0f;
}

void RayMarching::Draw(const GameTimer& gt)
{
	ThrowIfFailed(mCommandListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(mCommandListAlloc.Get(), mPSO.Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	));

	mCommandList->ClearRenderTargetView(
		CurrentBackBufferView(),
		Colors::Black,
		0, nullptr
	);

	mCommandList->ClearDepthStencilView(
		DepthStencilView(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0,
		0, nullptr
	);

	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = mObjectCB->Resource()->GetGPUVirtualAddress();
	mCommandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

	mCommandList->IASetVertexBuffers(0, 1, &mGeometry->VertexBufferView());
	mCommandList->IASetIndexBuffer(&mGeometry->IndexBufferView());
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mCommandList->DrawIndexedInstanced(
		mGeometry->DrawArgs["screen"].IndexCount,
		1, 0, 0, 0
	);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT
	));

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	FlushCommandQueue();
}

void RayMarching::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void RayMarching::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void RayMarching::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		float dx = -XMConvertToRadians(.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(.25f * static_cast<float>(y - mLastMousePos.y));
		
		mTheta += dx;
		mPhi += dy;
		mPhi = MathHelper::Clamp(mPhi, .1f, MathHelper::Pi - .1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		float dx = .005f * static_cast<float>(x - mLastMousePos.x);
		float dy = .005f * static_cast<float>(y - mLastMousePos.y);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void RayMarching::BuildConstantBuffers()
{
	mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);
}

void RayMarching::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];

	slotRootParameter[0].InitAsConstantBufferView(0);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		1,
		slotRootParameter,
		0,
		nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	if (errorBlob != nullptr)
	{
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature)
	));
}

void RayMarching::BuildShadersAndInputLayout()
{
	HRESULT hr = S_OK;

	mvsByteCode = d3dUtil::CompileShader(L"Fractal.hlsl", nullptr, "VS", "vs_5_0");
	mpsByteCode = d3dUtil::CompileShader(L"Fractal.hlsl", nullptr, "PS", "ps_5_0");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void RayMarching::BuildGeometry()
{
	std::array<Vertex, 4> vertices =
	{
		Vertex({XMFLOAT3(-1.0f, +1.0f, .0f)}),
		Vertex({XMFLOAT3(+1.0f, +1.0f, .0f)}),
		Vertex({XMFLOAT3(+1.0f, -1.0f, .0f)}),
		Vertex({XMFLOAT3(-1.0f, -1.0f, .0f)})
	};

	std::array<std::uint16_t, 6> indices =
	{
		0, 1, 2,
		2, 3, 0
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mGeometry = std::make_unique<MeshGeometry>();

	mGeometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(),
		mCommandList.Get(),
		vertices.data(),
		vbByteSize,
		mGeometry->VertexBufferUploader
	);

	mGeometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(),
		mCommandList.Get(),
		indices.data(),
		ibByteSize,
		mGeometry->IndexBufferUploader
	);

	mGeometry->VertexByteStride = sizeof(Vertex);
	mGeometry->VertexBufferByteSize = vbByteSize;
	mGeometry->IndexFormat = DXGI_FORMAT_R16_UINT;
	mGeometry->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	mGeometry->DrawArgs["screen"] = submesh;
}

void RayMarching::BuildPSO()
{
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
	
	D3D12_RASTERIZER_DESC rastDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rastDesc.CullMode = D3D12_CULL_MODE_BACK;
	psoDesc.RasterizerState = rastDesc;

	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}
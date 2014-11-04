//--------------------------------------------------------------------------------------
// File: TessellationDemoD3D11.cpp
//
// This application demonstrates simple lighting in the vertex shader
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>
#include "resource.h"


//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
	XMFLOAT3 Pos;
	XMFLOAT2 TexCoord;
	XMFLOAT3 Normal;
	//Binormal;
	//XMFLOAT3 Tangent;
};

struct Camera
{
	XMVECTOR Eye;
	XMVECTOR At;
	XMVECTOR Up;

	float Speed = 10.0f;

	bool isMovingForward = false;
	bool isMovingBackward = false;
	bool isMovingLeft = false;
	bool isMovingRight = false;
	bool isMovingUp = false;
	bool isMovingDown = false;

};


struct ConstantBuffer
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
	XMFLOAT4 vPos;
	XMFLOAT4 LightPos;
	XMFLOAT4 LightColor;
	XMFLOAT4 Eye;
	float TessellationFactor;
	float Scaling;
	float DisplacementLevel;
};


//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE                           g_hInst = NULL;
HWND                                g_hWnd = NULL;
D3D_DRIVER_TYPE                     g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL                   g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*                       g_pd3dDevice = NULL;
ID3D11DeviceContext*                g_pImmediateContext = NULL;
IDXGISwapChain*                     g_pSwapChain = NULL;
ID3D11RenderTargetView*             g_pRenderTargetView = NULL;
ID3D11Texture2D*                    g_pDepthStencil = NULL;
ID3D11DepthStencilView*             g_pDepthStencilView = NULL;
ID3D11VertexShader*                 g_pVertexShader = NULL;
ID3D11HullShader*                   g_pHullShader = NULL;
ID3D11DomainShader*                 g_pDomainShader = NULL;
ID3D11PixelShader*                  g_pPixelShader = NULL;
ID3D11PixelShader*                  g_pSolidPixelShader = NULL;
ID3D11InputLayout*                  g_pVertexLayout = NULL;
ID3D11Buffer*                       g_pVertexBuffer = NULL;
ID3D11Buffer*                       g_pIndexBuffer = NULL;
ID3D11Buffer*                       g_pConstantBuffer = NULL;
ID3D11ShaderResourceView*           g_pDiffuseTextureRV = NULL;
ID3D11ShaderResourceView*           g_pDispTextureRV = NULL;
ID3D11ShaderResourceView*           g_pNormTextureRV = NULL;
ID3D11SamplerState*                 g_pSamplerPoint = NULL;
ID3D11SamplerState*                 g_pSamplerLinear = NULL;
ID3D11RasterizerState*              g_pWireFrameRasterizerState = NULL;
ID3D11RasterizerState*              g_pDefaultRasterizerState = NULL;
XMMATRIX                            g_World;
XMMATRIX                            g_View;
XMMATRIX                            g_Projection;
Camera                              g_Camera;
bool                                g_IsWireFrame = false;
float                               g_TessellationFactor = 64.0f;
float                               g_Scaling = 3.0f;
float                               g_DisplacementLevel = 0.1f;


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void Render();


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	if (FAILED(InitWindow(hInstance, nCmdShow)))
		return 0;

	if (FAILED(InitDevice()))
	{
		CleanupDevice();
		return 0;
	}

	// Main message loop
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			Render();
		}
	}

	CleanupDevice();

	return (int)msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_TUTORIAL1);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"WindowClass1";
	wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_TUTORIAL1);
	if (!RegisterClassEx(&wcex))
		return E_FAIL;

	// Create window
	g_hInst = hInstance;
	RECT rc = { 0, 0, 1600, 900 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindow(L"WindowClass1", L"Direct3D Tessellation", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
		NULL);
	if (!g_hWnd)
		return E_FAIL;

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DX11
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile(WCHAR* szFileName, D3D10_SHADER_MACRO* pDefines, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob;
	hr = D3DX11CompileFromFile(szFileName, pDefines, NULL, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL);
	if (FAILED(hr))
	{
		if (pErrorBlob != NULL)
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		if (pErrorBlob) pErrorBlob->Release();
		return hr;
	}
	if (pErrorBlob) pErrorBlob->Release();

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = g_hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDeviceAndSwapChain(NULL, g_driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
		if (SUCCEEDED(hr))
			break;
	}
	if (FAILED(hr))
		return hr;

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = NULL;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	if (FAILED(hr))
		return hr;

	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = g_pd3dDevice->CreateTexture2D(&descDepth, NULL, &g_pDepthStencil);
	if (FAILED(hr))
		return hr;

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
	if (FAILED(hr))
		return hr;

	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

	// Create wireframe rasterizer state
	D3D11_RASTERIZER_DESC wfdesc;
	ZeroMemory(&wfdesc, sizeof(D3D11_RASTERIZER_DESC));
	wfdesc.FillMode = D3D11_FILL_WIREFRAME;
	wfdesc.CullMode = D3D11_CULL_NONE;
	hr = g_pd3dDevice->CreateRasterizerState(&wfdesc, &g_pWireFrameRasterizerState);

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports(1, &vp);

	// Compile the vertex shader
	ID3DBlob* pVSBlob = NULL;
	hr = CompileShaderFromFile(L"Shaders/DisplacedAndShaded.hlsl", NULL, "VS", "vs_5_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
			L"The vertex shader cannot be compiled.", L"Error", MB_OK);
		return hr;
	}

	// Create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShader);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	// Define the input layout
	/*D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};*/
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	UINT numElements = ARRAYSIZE(layout);

	// Create the input layout
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pVertexLayout);
	pVSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Set the input layout
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	// Compile the hull shader
	ID3DBlob* pHSBlob = NULL;
	hr = CompileShaderFromFile(L"Shaders/DisplacedAndShaded.hlsl", NULL, "HS", "hs_5_0", &pHSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
			L"The hull shader cannot be compiled.", L"Error", MB_OK);
		return hr;
	}

	// Create the hull shader
	hr = g_pd3dDevice->CreateHullShader(pHSBlob->GetBufferPointer(), pHSBlob->GetBufferSize(), NULL, &g_pHullShader);
	pHSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Compile the domain shader
	ID3DBlob* pDSBlob = NULL;
	hr = CompileShaderFromFile(L"Shaders/DisplacedAndShaded.hlsl", NULL, "DS", "ds_5_0", &pDSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
			L"The domain shader cannot be compiled.", L"Error", MB_OK);
		return hr;
	}

	// Create the domain shader
	hr = g_pd3dDevice->CreateDomainShader(pDSBlob->GetBufferPointer(), pDSBlob->GetBufferSize(), NULL, &g_pDomainShader);
	pDSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Compile the pixel shader
	ID3DBlob* pPSBlob = NULL;
	hr = CompileShaderFromFile(L"Shaders/DisplacedAndShaded.hlsl", NULL, "PS", "ps_5_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
			L"The pixel shader cannot be compiled.", L"Error", MB_OK);
		return hr;
	}

	// Create the pixel shader
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShader);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Compile the solid color pixel shader
	ID3DBlob* pSolidPSBlob = NULL;
	hr = CompileShaderFromFile(L"Shaders/DisplacedAndShaded.hlsl", NULL, "SolidPS", "ps_5_0", &pSolidPSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
			L"The solid pixel shader cannot be compiled.", L"Error", MB_OK);
		return hr;
	}

	// Create the pixel shader
	hr = g_pd3dDevice->CreatePixelShader(pSolidPSBlob->GetBufferPointer(), pSolidPSBlob->GetBufferSize(), NULL, &g_pSolidPixelShader);
	pSolidPSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Create vertex buffer
	XMFLOAT3 planeBinormal = XMFLOAT3(1.0f, 0.0f, 0.0f);
	XMFLOAT3 planeTangent = XMFLOAT3(0.0f, 0.0f, 1.0f);
	SimpleVertex vertices[] =
	{
		{ XMFLOAT3(-1.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)/*, planeBinormal, planeTangent */},
		{ XMFLOAT3( 1.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)/*, planeBinormal, planeTangent */},
		{ XMFLOAT3( 1.0f, 0.0f,  1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)/*, planeBinormal, planeTangent */},
		{ XMFLOAT3(-1.0f, 0.0f,  1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)/*, planeBinormal, planeTangent */}
	};

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex)* ARRAYSIZE(vertices);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = vertices;
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
	if (FAILED(hr))
		return hr;

	// Set vertex buffer
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// Create index buffer
	WORD indices[] =
	{
		3, 2, 0,
		0, 2, 1
	};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(WORD)* ARRAYSIZE(indices);
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	InitData.pSysMem = indices;
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pIndexBuffer);
	if (FAILED(hr))
		return hr;

	// Set index buffer
	g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	// Set primitive topology
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);

	// Create the constant buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(ConstantBuffer);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, NULL, &g_pConstantBuffer);
	if (FAILED(hr))
		return hr;

	// Load the Diffuse texture
	hr = D3DX11CreateShaderResourceViewFromFile(g_pd3dDevice,
		L"Textures/Diffuse/rock_diffuse.jpg", NULL, NULL, &g_pDiffuseTextureRV, NULL);
	if (FAILED(hr))
		return hr;

	// Load the Displacement texture
	hr = D3DX11CreateShaderResourceViewFromFile(g_pd3dDevice,
		L"Textures/Displacement/rock_displacement.jpg", NULL, NULL, &g_pDispTextureRV, NULL);
	if (FAILED(hr))
		return hr;

	// Load the Normal texture
	hr = D3DX11CreateShaderResourceViewFromFile(g_pd3dDevice,
		L"Textures/Normal/rock_normal.jpg", NULL, NULL, &g_pNormTextureRV, NULL);
	if (FAILED(hr))
		return hr;

	// Create the point sampler state
	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = g_pd3dDevice->CreateSamplerState(&sampDesc, &g_pSamplerPoint);
	if (FAILED(hr))
		return hr;

	// Create the linear sampler state
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = g_pd3dDevice->CreateSamplerState(&sampDesc, &g_pSamplerLinear);
	if (FAILED(hr))
		return hr;

	// Initialize the world matrices
	g_World = XMMatrixIdentity();

	// Initialize the view matrix
	g_Camera.Eye = XMVectorSet(0.0f, 4.0f, -10.0f, 0.0f);
	g_Camera.At = XMVectorSet(1.5f, 0.0f, 0.0f, 0.0f);
	g_Camera.Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	g_View = XMMatrixLookAtLH(g_Camera.Eye, g_Camera.At, g_Camera.Up);

	// Initialize the projection matrix
	g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, width / (FLOAT)height, 0.01f, 100.0f);

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
	if (g_pImmediateContext) g_pImmediateContext->ClearState();

	if (g_pConstantBuffer) g_pConstantBuffer->Release();
	if (g_pVertexBuffer) g_pVertexBuffer->Release();
	if (g_pIndexBuffer) g_pIndexBuffer->Release();
	if (g_pVertexLayout) g_pVertexLayout->Release();
	if (g_pVertexShader) g_pVertexShader->Release();
	if (g_pHullShader) g_pHullShader->Release();
	if (g_pDomainShader) g_pDomainShader->Release();
	if (g_pPixelShader) g_pPixelShader->Release();
	if (g_pSolidPixelShader) g_pSolidPixelShader->Release();
	if (g_pDepthStencil) g_pDepthStencil->Release();
	if (g_pDepthStencilView) g_pDepthStencilView->Release();
	if (g_pRenderTargetView) g_pRenderTargetView->Release();
	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();
	if (g_pDiffuseTextureRV) g_pDiffuseTextureRV->Release();
	if (g_pDispTextureRV) g_pDispTextureRV->Release();
	if (g_pNormTextureRV) g_pNormTextureRV->Release();
	if (g_pSamplerPoint) g_pSamplerPoint->Release();
	if (g_pSamplerLinear) g_pSamplerLinear->Release();
	if (g_pWireFrameRasterizerState) g_pWireFrameRasterizerState->Release();
	if (g_pDefaultRasterizerState) g_pDefaultRasterizerState->Release();
}


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_KEYDOWN:
		if (wParam == 'W')
			g_Camera.isMovingForward = true;
		if (wParam == 'S')
			g_Camera.isMovingBackward = true;
		if (wParam == 'A')
			g_Camera.isMovingLeft = true;
		if (wParam == 'D')
			g_Camera.isMovingRight = true;
		if (wParam == VK_SPACE)
			g_Camera.isMovingUp = true;
		if (wParam == VK_CONTROL)
			g_Camera.isMovingDown = true;
		if (wParam == 'R')
		{
			g_pImmediateContext->RSGetState(&g_pDefaultRasterizerState);
			if (!g_pDefaultRasterizerState)
			{
				g_IsWireFrame = true;
				g_pImmediateContext->RSSetState(g_pWireFrameRasterizerState);
			}
			else
			{
				g_IsWireFrame = false;
				g_pImmediateContext->RSSetState(NULL);
			}
		}
		if (wParam == VK_UP && g_TessellationFactor <= 64.0f)
			g_TessellationFactor += 0.5f;
		if (wParam == VK_DOWN && g_TessellationFactor >= 1.0f)
			g_TessellationFactor -= 0.5f;
		if (wParam == VK_ESCAPE)
			PostQuitMessage(0);
		break;
	case WM_KEYUP:
		if (wParam == 'W')
			g_Camera.isMovingForward = false;
		if (wParam == 'S')
			g_Camera.isMovingBackward = false;
		if (wParam == 'A')
			g_Camera.isMovingLeft = false;
		if (wParam == 'D')
			g_Camera.isMovingRight = false;
		if (wParam == VK_SPACE)
			g_Camera.isMovingUp = false;
		if (wParam == VK_CONTROL)
			g_Camera.isMovingDown = false;
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}


//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
	// Update our time
	static float t = 0.0f;
	static float oldt = 0.0f;
	static float dt = 0.0f;
	if (g_driverType == D3D_DRIVER_TYPE_REFERENCE)
	{
		t += (float)XM_PI * 0.0125f;
	}
	else
	{
		static DWORD dwTimeStart = 0;
		DWORD dwTimeCur = GetTickCount();
		if (dwTimeStart == 0)
			dwTimeStart = dwTimeCur;
		oldt = t;
		t = (dwTimeCur - dwTimeStart) / 1000.0f;
		dt = t - oldt;
	}

	// Update camera and view matrix
	if (g_Camera.isMovingForward)
		g_Camera.Eye = XMVectorAdd(g_Camera.Eye, XMVector4Normalize(XMVectorSubtract(g_Camera.At, g_Camera.Eye)) * g_Camera.Speed * dt);
	if (g_Camera.isMovingBackward)
		g_Camera.Eye = XMVectorAdd(g_Camera.Eye, XMVector4Normalize(XMVectorSubtract(g_Camera.At, g_Camera.Eye)) * g_Camera.Speed * dt * -1.0f);
	if (g_Camera.isMovingLeft)
		g_Camera.Eye = XMVectorAdd(g_Camera.Eye, XMVector4Normalize(XMVector3Cross(g_Camera.Up, XMVectorSubtract(g_Camera.At, g_Camera.Eye))) * g_Camera.Speed * dt * -1.0f);
	if (g_Camera.isMovingRight)
		g_Camera.Eye = XMVectorAdd(g_Camera.Eye, XMVector4Normalize(XMVector3Cross(g_Camera.Up, XMVectorSubtract(g_Camera.At, g_Camera.Eye))) * g_Camera.Speed * dt);
	if (g_Camera.isMovingUp)
		g_Camera.Eye = XMVectorAdd(g_Camera.Eye, XMVector4Normalize(g_Camera.Up) * g_Camera.Speed * dt);
	if (g_Camera.isMovingDown)
		g_Camera.Eye = XMVectorAdd(g_Camera.Eye, XMVector4Normalize(g_Camera.Up) * g_Camera.Speed * dt * -1.0f);
	g_View = XMMatrixLookAtLH(g_Camera.Eye, g_Camera.At, g_Camera.Up);

	// Setup our lighting parameters
	XMFLOAT4 LightPos = XMFLOAT4(-10, 10, 10, 1.0f);
	XMFLOAT4 LightColor = XMFLOAT4(1, 1, 1, 1);

	//
	// Clear the back buffer
	//
	float ClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f }; // red, green, blue, alpha
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);

	//
	// Clear the depth buffer to 1.0 (max depth)
	//
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	//
	// Update matrix variables and lighting variables
	//
	ConstantBuffer cb1;
	cb1.mWorld = XMMatrixTranspose(g_World) * XMMatrixScaling(g_Scaling * 1.5f, g_Scaling, g_Scaling);
	cb1.mView = XMMatrixTranspose(g_View);
	cb1.mProjection = XMMatrixTranspose(g_Projection);
	cb1.vPos = XMFLOAT4(0, 0, 0, 1);
	cb1.LightPos = LightPos;
	cb1.LightColor = LightColor;
	XMStoreFloat4(&cb1.Eye, g_Camera.Eye);
	cb1.TessellationFactor = g_TessellationFactor;
	cb1.Scaling = g_Scaling;
	cb1.DisplacementLevel = g_DisplacementLevel;
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, NULL, &cb1, 0, 0);

	//
	// Render the quad
	//
	g_pImmediateContext->VSSetShader(g_pVertexShader, NULL, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);

	g_pImmediateContext->HSSetShader(g_pHullShader, NULL, 0);
	g_pImmediateContext->HSSetConstantBuffers(0, 1, &g_pConstantBuffer);

	g_pImmediateContext->DSSetShader(g_pDomainShader, NULL, 0);
	g_pImmediateContext->DSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pImmediateContext->DSSetShaderResources(1, 1, &g_pDispTextureRV);
	g_pImmediateContext->DSSetSamplers(0, 1, &g_pSamplerPoint);

	if (!g_IsWireFrame)
	{
		g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);
		g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer);
		g_pImmediateContext->PSSetShaderResources(0, 1, &g_pDiffuseTextureRV);
		g_pImmediateContext->PSSetShaderResources(2, 1, &g_pNormTextureRV);
		g_pImmediateContext->PSSetSamplers(1, 1, &g_pSamplerLinear);
	}
	else if (g_IsWireFrame)
	{
		g_pImmediateContext->PSSetShader(g_pSolidPixelShader, NULL, 0);
	}

	g_pImmediateContext->DrawIndexed(6, 0, 0);

	//
	// Present our back buffer to our front buffer
	//
	g_pSwapChain->Present(0, 0);
}



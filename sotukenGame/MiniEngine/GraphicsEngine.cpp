#include "stdafx.h"
#include "GraphicsEngine.h"

GraphicsEngine* g_graphicsEngine = nullptr;	//グラフィックスエンジン
Camera* g_camera2D = nullptr;				//2Dカメラ。
Camera* g_camera3D = nullptr;				//3Dカメラ。

GraphicsEngine::~GraphicsEngine()
{
	WaitDraw();
	//後始末。
	if (m_commandQueue) {
		m_commandQueue->Release();
	}
	if (m_swapChain) {
		m_swapChain->Release();
	}
	if (m_rtvHeap) {
		m_rtvHeap->Release();
	}
	if (m_dsvHeap) {
		m_dsvHeap->Release();
	}
	if (m_commandAllocator) {
		m_commandAllocator->Release();
	}
	if (m_commandList) {
		m_commandList->Release();
	}
	if (m_pipelineState) {
		m_pipelineState->Release();
	}
	for (auto& rt : m_renderTargets) {
		if (rt) {
			rt->Release();
		}
	}
	if (m_depthStencilBuffer) {
		m_depthStencilBuffer->Release();
	}
	if (m_fence) {
		m_fence->Release();
	}

	if (m_d3dDevice) {
		m_d3dDevice->Release();
	}

	CloseHandle(m_fenceEvent);
}
void GraphicsEngine::WaitDraw()
{
	//描画終了待ち
	// Signal and increment the fence value.
	const UINT64 fence = m_fenceValue;
	m_commandQueue->Signal(m_fence, fence);
	m_fenceValue++;

	// Wait until the previous frame is finished.
	if (m_fence->GetCompletedValue() < fence)
	{
		m_fence->SetEventOnCompletion(fence, m_fenceEvent);
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}
}
bool GraphicsEngine::Init(HWND hwnd, UINT frameBufferWidth, UINT frameBufferHeight)
{
	m_frameBufferWidth = frameBufferWidth;
	m_frameBufferHeight = frameBufferHeight;

	//デバイスにアクセスするためのインターフェースを作成。
	auto dxgiFactory = CreateDXGIFactory();
	//D3Dデバイスの作成。
	if (!CreateD3DDevice( dxgiFactory ) ) {
		//D3Dデバイスの作成に失敗した。
		MessageBox(hwnd, TEXT("D3Dデバイスの作成に失敗しました。"), TEXT("エラー"), MB_OK);
		return false;
	}
	//コマンドキューの作成。
	if (!CreateCommandQueue()) {
		//コマンドキューの作成に失敗した。
		MessageBox(hwnd, TEXT("コマンドキューの作成に失敗しました。"), TEXT("エラー"), MB_OK);
		return false;
	}
	//スワップチェインを作成。
	if (!CreateSwapChain(hwnd, frameBufferWidth, frameBufferHeight, dxgiFactory)) {
		//スワップチェインの作成に失敗。
		MessageBox(hwnd, TEXT("スワップチェインの作成に失敗しました。"), TEXT("エラー"), MB_OK);
		return false;
	}
	
	//フレームバッファ用のディスクリプタヒープを作成する。
	if (!CreateDescriptorHeapForFrameBuffer()) {
		MessageBox(hwnd, TEXT("フレームバッファ用のディスクリプタヒープの作成に失敗しました。"), TEXT("エラー"), MB_OK);
		return false;
	}

	//フレームバッファ用のRTVを作成する。
	if (!CreateRTVForFameBuffer()) {
		MessageBox(hwnd, TEXT("フレームバッファ用のRTVの作成に失敗しました。"), TEXT("エラー"), MB_OK);
		return false;

	}

	//フレームバッファ用のDSVを作成する。
	if (!CreateDSVForFrameBuffer(frameBufferWidth, frameBufferHeight)) {
		MessageBox(hwnd, TEXT("フレームバッファ用のDSVの作成に失敗しました。"), TEXT("エラー"), MB_OK);
		return false;
	}

	//コマンドアロケータの作成。
	m_d3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&m_commandAllocator));

	if (!m_commandAllocator) {
		MessageBox(hwnd, TEXT("コマンドアロケータの作成に失敗しました。"), TEXT("エラー"), MB_OK);
		return false;
	}

	//コマンドリストの作成。
	if (!CreateCommandList()) {
		MessageBox(hwnd, TEXT("コマンドリストの作成に失敗しました。"), TEXT("エラー"), MB_OK);
		return false;
	}

	//GPUと同期をとるためのオブジェクトを作成する。
	if (!CreateSynchronizationWithGPUObject()) {
		MessageBox(hwnd, TEXT("GPUと同期をとるためのオブジェクトの作成に失敗しました。"), TEXT("エラー"), MB_OK);
		return false;
	}
	
	//レンダリングコンテキストの作成。
	m_renderContext.Init(m_commandList);

	//ビューポートを初期化。
	m_viewport.TopLeftX = 0;
	m_viewport.TopLeftY = 0;
	m_viewport.Width = static_cast<FLOAT>(frameBufferWidth);
	m_viewport.Height = static_cast<FLOAT>(frameBufferHeight);
	m_viewport.MinDepth = D3D12_MIN_DEPTH;
	m_viewport.MaxDepth = D3D12_MAX_DEPTH;

	//シザリング矩形を初期化。
	m_scissorRect.left = 0;
	m_scissorRect.top = 0;
	m_scissorRect.right = frameBufferWidth;
	m_scissorRect.bottom = frameBufferHeight;

	//CBR_SVRのディスクリプタのサイズを取得。
	m_cbrSrvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//Samplerのディスクリプタのサイズを取得。
	m_samplerDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	//初期化が終わったのでDXGIを破棄。
	dxgiFactory->Release();

	//ヌルテクスチャを初期化
	m_nullTextureMaps.Init();

	//カメラを初期化する。
	m_camera2D.SetUpdateProjMatrixFunc(Camera::enUpdateProjMatrixFunc_Ortho);
	m_camera2D.SetWidth( static_cast<float>(m_frameBufferWidth) );
	m_camera2D.SetHeight( static_cast<float>(m_frameBufferHeight) );
	m_camera2D.SetPosition({0.0f, 0.0f, 1.0f});
	m_camera2D.SetTarget({ 0.0f, 0.0f, 0.0f });

	m_camera3D.SetPosition({0.0f, 50.0f, 200.0f} );
	m_camera3D.SetTarget({ 0.0f, 50.0f, 0.0f });

	g_camera2D = &m_camera2D;
	g_camera3D = &m_camera3D;

	m_shadowMap = new ShadowMap();
	m_cascadeShadowMap = new CascadeShadowMap();
	
	//ここからディファードレンダリングのための準備。

	//ディレクションライトの設定。
	Vector3 lightDir = Vector3(1.0f,-1.0f,0.0f);
	lightDir.Normalize();

	m_dirLight.direction = Vector4(lightDir.x, lightDir.y, lightDir.z, 1.0f);
	m_dirLight.lightcolor = { 0.5f, 0.5f, 0.5f, 1.0f };

	float color3[4] = { 0.5f,0.5f,0.5f,1.0f };

	//メインレンダリングターゲット
	m_mainRenderTarget.Create(
		FRAME_BUFFER_W,
		FRAME_BUFFER_H,
		1,
		1,
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		DXGI_FORMAT_D32_FLOAT,
		color3
	);

	float color[4] = { 1.0f,1.0f,1.0f,1.0f };
	float color2[4] = { 0.0f,0.0f,0.0f,1.0f };

	//ディファードレンダリングのためのレンダリングターゲットを初期するぜ
	m_albedRT.Create(
		FRAME_BUFFER_W,
		FRAME_BUFFER_H,
		1,
		1,
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		DXGI_FORMAT_D32_FLOAT,
		color
	);

	m_albedRT.SetClearColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));

	m_normalRT.Create(
		FRAME_BUFFER_W,
		FRAME_BUFFER_H,
		1,
		1,
		DXGI_FORMAT_B8G8R8A8_UNORM,
		DXGI_FORMAT_UNKNOWN,
		color2
	);

	m_worldPosRT.Create(
		FRAME_BUFFER_W,
		FRAME_BUFFER_H,
		1,
		1,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		DXGI_FORMAT_UNKNOWN,
		color
	);

	m_depthRT.Create(
		FRAME_BUFFER_W,
		FRAME_BUFFER_H,
		1,
		1,
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		DXGI_FORMAT_UNKNOWN,
		color
	);

	m_shadowColorRT.Create(
		FRAME_BUFFER_W,
		FRAME_BUFFER_H,
		1,
		1,
		DXGI_FORMAT_R32_FLOAT,
		DXGI_FORMAT_UNKNOWN,
		color
	);

	m_shadowColorRT.SetClearColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));


	//ディファードレンダリング用のテクスチャを作成しまーす
	//テクスチャデータの設定をする
	SpriteInitData spriteInitData;
	spriteInitData.m_width = FRAME_BUFFER_W;
	spriteInitData.m_height = FRAME_BUFFER_H;
	//アルベド
	spriteInitData.m_textures[0] = &m_albedRT.GetRenderTargetTexture();
	//法線
	spriteInitData.m_textures[1] = &m_normalRT.GetRenderTargetTexture();
	//深度
	//spriteInitData.m_textures[2] = &m_depthRT.GetRenderTargetTexture();
	//ワールド座標
	spriteInitData.m_textures[2] = &m_shadowColorRT.GetRenderTargetTexture();
	//ディファードレンダリング専用のシェーダー使う
	spriteInitData.m_fxFilePath = "Assets/shader/deferred.fx";
	spriteInitData.m_expandConstantBuffer = &m_dirLight;
	spriteInitData.m_expandConstantBufferSize = sizeof(m_dirLight);
	m_defferdSprite.Init(spriteInitData);

	spriteInitData.m_textures[0] = &m_mainRenderTarget.GetRenderTargetTexture();
	spriteInitData.m_fxFilePath = "Assets/shader/sprite.fx";

	m_copyMainRtToFrameBufferSprite.Init(spriteInitData);

	m_postEffect.Init();


	return true;
}
IDXGIFactory4* GraphicsEngine::CreateDXGIFactory()
{
	UINT dxgiFactoryFlags = 0;
#ifdef _DEBUG
	//デバッグコントローラーがあれば、デバッグレイヤーがあるDXGIを作成する。
	ID3D12Debug* debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();

		// Enable additional debug layers.
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		debugController->Release();
	}
#endif
	IDXGIFactory4* factory;
	CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));
	return factory;
}

bool GraphicsEngine::CreateD3DDevice( IDXGIFactory4* dxgiFactory )
{
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_1,	//Direct3D 12.1の機能を使う。
		D3D_FEATURE_LEVEL_12_0	//Direct3D 12.0の機能を使う。
	};

	IDXGIAdapter* adapterTmp = nullptr;
	IDXGIAdapter* adapterVender[Num_GPUVender] = { nullptr };	//各ベンダーのアダプター。
	IDXGIAdapter* adapterMaxVideoMemory = nullptr;				//最大ビデオメモリのアダプタ。
	IDXGIAdapter* useAdapter = nullptr;							//最終的に使用するアダプタ。
	SIZE_T videoMemorySize = 0;
	for (int i = 0; dxgiFactory->EnumAdapters(i, &adapterTmp) != DXGI_ERROR_NOT_FOUND; i++) {
		DXGI_ADAPTER_DESC desc;
		adapterTmp->GetDesc(&desc);
		
		if (desc.DedicatedVideoMemory > videoMemorySize) {
			//こちらのビデオメモリの方が多いので、こちらを使う。
			adapterMaxVideoMemory = adapterTmp;
			videoMemorySize = desc.DedicatedVideoMemory;
		}
		if (wcsstr(desc.Description, L"NVIDIA") != nullptr) {
			//NVIDIA製
			adapterVender[GPU_VenderNvidia] = adapterTmp;
		}
		else if (wcsstr(desc.Description, L"AMD") != nullptr) {
			//AMD製
			adapterVender[GPU_VenderAMD] = adapterTmp;
		}
		else if (wcsstr(desc.Description, L"Intel") != nullptr) {
			//Intel製
			adapterVender[GPU_VenderIntel] = adapterTmp;
		}
	}
	//使用するアダプターを決める。
	if (adapterVender[GPU_VenderNvidia] != nullptr) {
		//NVIDIA製が最優先
		useAdapter = adapterVender[GPU_VenderNvidia];
	}
	else if (adapterVender[GPU_VenderAMD] != nullptr) {
		//次はAMDが優先。
		useAdapter = adapterVender[GPU_VenderAMD];
	}
	else{
		//NVIDIAとAMDのGPUがなければビデオメモリが一番多いやつを使う。
		useAdapter = adapterMaxVideoMemory;
	}
	for (auto featureLevel : featureLevels) {

		auto hr = D3D12CreateDevice(
			useAdapter,
			featureLevel,
			IID_PPV_ARGS(&m_d3dDevice)
		);
		if (SUCCEEDED(hr)) {
			//D3Dデバイスの作成に成功した。
			break;
		}
	}
	return m_d3dDevice != nullptr;
}
bool GraphicsEngine::CreateCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	auto hr = m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
	if (FAILED(hr)) {
		//コマンドキューの作成に失敗した。
		return false;
	}
	return true;
}
bool GraphicsEngine::CreateSwapChain(
	HWND hwnd,
	UINT frameBufferWidth,
	UINT frameBufferHeight,
	IDXGIFactory4* dxgiFactory
)
{
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FRAME_BUFFER_COUNT;
	swapChainDesc.Width = frameBufferWidth;
	swapChainDesc.Height = frameBufferHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	IDXGISwapChain1* swapChain;
	dxgiFactory->CreateSwapChainForHwnd(
		m_commandQueue,
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
	);
	//IDXGISwapChain3のインターフェースを取得。
	swapChain->QueryInterface(IID_PPV_ARGS(&m_swapChain));
	swapChain->Release();
	//IDXGISwapChain3のインターフェースを取得。
	m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
	return true;
}
bool GraphicsEngine::CreateDescriptorHeapForFrameBuffer()
{
	//RTV用のディスクリプタヒープを作成する。
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = FRAME_BUFFER_COUNT;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	auto hr = m_d3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvHeap));
	if (FAILED(hr)) {
		//RTV用のディスクリプタヒープの作成に失敗した。
		return false;
	}
	//ディスクリプタのサイズを取得。
	m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	//DSV用のディスクリプタヒープを作成する。
	desc.NumDescriptors = 1;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	hr = m_d3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_dsvHeap));
	if (FAILED(hr)) {
		//DSV用のディスクリプタヒープの作成に失敗した。
		return false;
	}
	//ディスクリプタのサイズを取得。
	m_dsvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	return true;
}
bool GraphicsEngine::CreateRTVForFameBuffer()
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

	//フロントバッファをバックバッファ用のRTVを作成。
	for (UINT n = 0; n < FRAME_BUFFER_COUNT; n++) {
		m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n]));
		m_d3dDevice->CreateRenderTargetView(
			m_renderTargets[n], nullptr, rtvHandle
		);
		rtvHandle.ptr += m_rtvDescriptorSize;
	}
	return true;
}
bool GraphicsEngine::CreateDSVForFrameBuffer( UINT frameBufferWidth, UINT frameBufferHeight )
{
	D3D12_CLEAR_VALUE dsvClearValue;
	dsvClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	dsvClearValue.DepthStencil.Depth = 1.0f;
	dsvClearValue.DepthStencil.Stencil = 0;

	CD3DX12_RESOURCE_DESC desc(
		D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		0,
		frameBufferWidth,
		frameBufferHeight,
		1,
		1,
		DXGI_FORMAT_D32_FLOAT,
		1,
		0,
		D3D12_TEXTURE_LAYOUT_UNKNOWN,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);

	auto hr = m_d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&dsvClearValue,
		IID_PPV_ARGS(&m_depthStencilBuffer)
	);
	if (FAILED(hr)) {
		//深度ステンシルバッファの作成に失敗。
		return false;
	}
	//ディスクリプタを作成
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	m_d3dDevice->CreateDepthStencilView(
		m_depthStencilBuffer, nullptr, dsvHandle
	);

	return true;
}
bool GraphicsEngine::CreateCommandList()
{
	//コマンドリストの作成。
	m_d3dDevice->CreateCommandList(
		0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator, nullptr, IID_PPV_ARGS(&m_commandList)
	);
	if (!m_commandList) {
		return false;
	}
	//コマンドリストは開かれている状態で作成されるので、いったん閉じる。
	m_commandList->Close();

	return true;
}
bool GraphicsEngine::CreateSynchronizationWithGPUObject()
{
	m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
	if (!m_fence) {
		//フェンスの作成に失敗した。
		return false;
	}
	m_fenceValue = 1;
	//同期を行うときのイベントハンドラを作成する。
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fenceEvent == nullptr) {
		return false;
	}
	return true;
}
void GraphicsEngine::BeginRender()
{
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	//カメラを更新する。
	m_camera2D.Update();
	m_camera3D.Update();

	//コマンドアロケータををリセット。
	m_commandAllocator->Reset();
	//レンダリングコンテキストもリセット。
	m_renderContext.Reset(m_commandAllocator, m_pipelineState);
	//ビューポートを設定。
	//ビューポートを設定。
	m_renderContext.SetViewport(m_viewport);
	//シザリング矩形を設定。
	m_renderContext.SetScissorRect(m_scissorRect);

	m_currentFrameBufferRTVHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	m_currentFrameBufferRTVHandle.ptr += m_frameIndex * m_rtvDescriptorSize;
	//深度ステンシルバッファのディスクリプタヒープの開始アドレスを取得。
	m_currentFrameBufferDSVHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	//バックバッファがレンダリングターゲットとして設定可能になるまで待つ。
	m_renderContext.WaitUntilToPossibleSetRenderTarget(m_renderTargets[m_frameIndex]);

	//レンダリングターゲットを設定。
	m_renderContext.SetRenderTarget(m_currentFrameBufferRTVHandle, m_currentFrameBufferDSVHandle);

	const float clearColor[] = { 0.5f, 0.5f, 0.5f, 1.0f };
	m_renderContext.ClearRenderTargetView(m_currentFrameBufferRTVHandle, clearColor);
	m_renderContext.ClearDepthStencilView(m_currentFrameBufferDSVHandle, 1.0f);

}

void GraphicsEngine::EndRender()
{
	// レンダリングターゲットへの描き込み完了待ち
	m_renderContext.WaitUntilFinishDrawingToRenderTarget(m_renderTargets[m_frameIndex]);

	//レンダリングコンテキストを閉じる。
	m_renderContext.Close();

	//コマンドを実行。
	ID3D12CommandList* ppCommandLists[] = { m_commandList };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
#ifdef SAMPE_16_04
	// Present the frame.
	m_swapChain->Present(0, 0);
#else
	// Present the frame.
	m_swapChain->Present(1, 0);
#endif
	//描画完了待ち。
	WaitDraw();
}

void GraphicsEngine::BeginDeferredRender()
{
	RenderTarget* rt[] = {
		m_shadowMap->GetRenderTarget()
	};

	//レンダリングターゲットを設定
	RenderTarget* rts[] = {
		&m_albedRT,
		&m_normalRT,
		&m_shadowColorRT
	};

	//シャドウマップをシェーダーリソースビューとして使用したいので描き込み完了待ちする
	m_renderContext.WaitUntilFinishDrawingToRenderTargets(1, rt);

	//G-Bufferのための３つのレンダリングターゲットを待機完了状態にする
	m_renderContext.WaitUntilToPossibleSetRenderTargets(3, rts);

	m_renderContext.SetRenderTargets(3, rts);
	m_renderContext.SetViewport(m_albedRT);
	m_renderContext.ClearRenderTargetViews(3, rts);
}

void GraphicsEngine::EndModelDraw()
{
	//レンダリングターゲットを設定
	RenderTarget* rts[] = {
		&m_albedRT,
		&m_normalRT,
		&m_shadowColorRT
	};
	m_renderContext.WaitUntilFinishDrawingToRenderTargets(3, rts);

	RenderTarget* rt[] = {
		&m_mainRenderTarget
	};

	m_renderContext.WaitUntilToPossibleSetRenderTargets(1, rt);
	SetRenderTarget(1, rt);
	m_renderContext.ClearRenderTargetViews(1, rt);
	m_defferdSprite.Draw(m_renderContext);
}

void GraphicsEngine::RendertoPostEffect()
{
	//ポストエフェクトかけて(メインレンダリングターゲットに)
	m_postEffect.Render(m_renderContext);

	//レンダリングターゲットをフレームバッファにして
	ChangeRenderTargetToFrameBuffer(m_renderContext);

	//フレームバッファにドローする
	m_copyMainRtToFrameBufferSprite.Draw(m_renderContext);
}

void GraphicsEngine::RendertoShadow()
{
	m_shadowMap->UpdateFromLightTaraget(Vector3(300.0f, 300.0f, 200.0f), Vector3(0.0f, 0.0f, 0.0f));
	//m_shadowMap->UpdateFromLightTaraget(g_camera3D->GetPosition(), g_camera3D->GetTarget());
	//m_shadowMap->RenderToShadowMap(m_renderContext);
	m_cascadeShadowMap->Update();
	m_cascadeShadowMap->RenderToShadowMap(m_renderContext);
}

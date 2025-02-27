#include "stdafx.h"
#include "Sprite.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"

	namespace {
		struct SSimpleVertex {
			Vector4 pos;
			Vector2 tex;
		};
	}
	const Vector2	Sprite::DEFAULT_PIVOT = { 0.5f, 0.5f };
	Sprite::~Sprite()
	{
	}
	void Sprite::InitTextures(const SpriteInitData& initData)
	{
		//スプライトで使用するテクスチャを準備する。
		if (initData.m_ddsFilePath[0] != nullptr) {
			//ddsファイルのパスが指定されてるのなら、ddsファイルからテクスチャを作成する。
			int texNo = 0;
			while (initData.m_ddsFilePath[texNo] && texNo < MAX_TEXTURE) {
				wchar_t wddsFilePath[1024];
				mbstowcs(wddsFilePath, initData.m_ddsFilePath[texNo], 1023);
				m_textures[texNo].InitFromDDSFile(wddsFilePath);
				texNo++;
			}
			m_numTexture = texNo;
		}
		else if (initData.m_textures[0] != nullptr) {
			//外部テクスチャを指定されている。
			int texNo = 0;
			while (initData.m_textures[texNo] != nullptr) {
				m_textureExternal[texNo] = initData.m_textures[texNo];
				texNo++;
			}
			m_numTexture = texNo;
		}
		else {
			//テクスチャが指定されてない。
			MessageBoxA(nullptr, "initData.m_ddsFilePathかm_texturesのどちらかに使用するテクスチャの情報を設定してください。", "エラー", MB_OK);
			std::abort();
		}
	}
	void Sprite::InitShader(const SpriteInitData& initData)
	{
		if (initData.m_fxFilePath == nullptr) {
			MessageBoxA(nullptr, "fxファイルが指定されていません。", "エラー", MB_OK);
			std::abort();
		}
		wchar_t fxFilePath[1024];
		mbstowcs(fxFilePath, initData.m_fxFilePath, 1023);
		//シェーダーをロードする。
		m_vs.LoadVS(fxFilePath, initData.m_vsEntryPointFunc);
		m_ps.LoadPS(fxFilePath, initData.m_psEntryPoinFunc);
	}
	void Sprite::InitDescriptorHeap(const SpriteInitData& initData)
	{
		if (m_textureExternal[0] != nullptr) {
			//外部のテクスチャが指定されている。
			for (int texNo = 0; texNo < m_numTexture; texNo++) {
				m_descriptorHeap.RegistShaderResource(texNo, *m_textureExternal[texNo]);
			}
		}
		else {
			for (int texNo = 0; texNo < m_numTexture; texNo++) {
				m_descriptorHeap.RegistShaderResource(texNo, m_textures[texNo]);
			}
		}
		if (initData.m_expandShaderResoruceView != nullptr) {
			//拡張シェーダーリソースビュー。
			m_descriptorHeap.RegistShaderResource(
				EXPAND_SRV_REG__START_NO,
				*initData.m_expandShaderResoruceView
			);
		}
		m_descriptorHeap.RegistConstantBuffer(0, m_constantBufferGPU);
		if (m_userExpandConstantBufferCPU != nullptr) {
			//ユーザー拡張の定数バッファはb1に関連付けする。
			m_descriptorHeap.RegistConstantBuffer(1, m_userExpandConstantBufferGPU);
		}
		m_descriptorHeap.Commit();
	}
	void Sprite::InitVertexBufferAndIndexBuffer(const SpriteInitData& initData)
	{
		float halfW = m_size.x * 0.5f;
		float halfH = m_size.y * 0.5f;
		//頂点バッファのソースデータ。
		SSimpleVertex vertices[] =
		{
			{
				Vector4(-halfW, -halfH, 0.0f, 1.0f),
				Vector2(1.0f, 1.0f),
			},
			{
				Vector4(halfW, -halfH, 0.0f, 1.0f),
				Vector2(0.0f, 1.0f),
			},
			{
				Vector4(-halfW, halfH, 0.0f, 1.0f),
				Vector2(1.0f, 0.0f)
			},
			{
				Vector4(halfW, halfH, 0.0f, 1.0f),
				Vector2(0.0f, 0.0f)
			}

		};
		unsigned short indices[] = { 0,1,2,3 };

		m_vertexBuffer.Init(sizeof(vertices), sizeof(vertices[0]));
		m_vertexBuffer.Copy(vertices);

		m_indexBuffer.Init(sizeof(indices), sizeof(indices[0]));
		m_indexBuffer.Copy(indices);
	}
	void Sprite::InitPipelineState()
	{
		// 頂点レイアウトを定義する。
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		//パイプラインステートを作成。
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = { 0 };
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_vs.GetCompiledBlob());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_ps.GetCompiledBlob());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		m_pipelineState.Init(psoDesc);
	}
	void Sprite::InitConstantBuffer(const SpriteInitData& initData)
	{
		//定数バッファの初期化。
		m_constantBufferGPU.Init(sizeof(m_constantBufferCPU), nullptr);
		//ユーザー拡張の定数バッファが指定されている。
		if (initData.m_expandConstantBuffer != nullptr){
			m_userExpandConstantBufferCPU = initData.m_expandConstantBuffer;
			m_userExpandConstantBufferGPU.Init(
				initData.m_expandConstantBufferSize, 
				initData.m_expandConstantBuffer
			);
		}
	}

	void Sprite::Init(const SpriteInitData& initData)
	{
		m_size.x = static_cast<float>(initData.m_width);
		m_size.y = static_cast<float>(initData.m_height);

		//テクスチャを初期化。
		InitTextures(initData);
		//頂点バッファとインデックスバッファを初期化。
		InitVertexBufferAndIndexBuffer(initData);
		//定数バッファを初期化。
		InitConstantBuffer(initData);
		
		//ルートシグネチャの初期化。
		m_rootSignature.Init(
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP);

		//シェーダーを初期化。
		InitShader(initData);
		//パイプラインステートの初期化。
		InitPipelineState();
		//ディスクリプタヒープを初期化。
		InitDescriptorHeap(initData);
	}
	void Sprite::Update(const Vector3& pos, const Quaternion& rot, const Vector3& scale, const Vector2& pivot)
	{
		//ピボットを考慮に入れた平行移動行列を作成。
		//ピボットは真ん中が0.0, 0.0、左上が-1.0f, -1.0、右下が1.0、1.0になるようにする。
		Vector2 localPivot = pivot;
		localPivot.x -= 0.5f;
		localPivot.y -= 0.5f;
		localPivot.x *= -2.0f;
		localPivot.y *= -2.0f;
		//画像のハーフサイズを求める。
		Vector2 halfSize = m_size;
		halfSize.x *= 0.5f;
		halfSize.y *= 0.5f;
		Matrix mPivotTrans;

		mPivotTrans.MakeTranslation(
			{ halfSize.x * localPivot.x, halfSize.y * localPivot.y, 0.0f }
		);
		Matrix mTrans, mRot, mScale;
		mTrans.MakeTranslation(pos);
		mRot.MakeRotationFromQuaternion(rot);
		mScale.MakeScaling(scale);
		m_world = mPivotTrans * mScale;
		m_world = m_world * mRot;
		m_world = m_world * mTrans;
	}
	void Sprite::Draw(RenderContext& renderContext)
	{
		Matrix viewMatrix = g_camera2D->GetViewMatrix();
		Matrix projMatrix = g_camera2D->GetProjectionMatrix();

		m_constantBufferCPU.mvp = m_world * viewMatrix * projMatrix;
		m_constantBufferCPU.mulColor.x = m_mulColor.x;
		m_constantBufferCPU.mulColor.y = m_mulColor.y;
		m_constantBufferCPU.mulColor.z = m_mulColor.z;
		m_constantBufferCPU.mulColor.w = m_mulColor.w;
		m_constantBufferCPU.screenParam.x = g_camera3D->GetNear();
		m_constantBufferCPU.screenParam.y = g_camera3D->GetFar();
		m_constantBufferCPU.screenParam.z = FRAME_BUFFER_W;
		m_constantBufferCPU.screenParam.w = FRAME_BUFFER_H;

		//定数バッファを更新。
		m_constantBufferGPU.CopyToVRAM(&m_constantBufferCPU);
		if (m_userExpandConstantBufferCPU != nullptr) {
			m_userExpandConstantBufferGPU.CopyToVRAM(m_userExpandConstantBufferCPU);
		}
		//ルートシグネチャを設定。
		renderContext.SetRootSignature(m_rootSignature);
		//パイプラインステートを設定。
		renderContext.SetPipelineState(m_pipelineState);
		//頂点バッファを設定。
		renderContext.SetVertexBuffer(m_vertexBuffer);
		//インデックスバッファを設定。
		renderContext.SetIndexBuffer(m_indexBuffer);
		//プリミティブトポロジーを設定する。
		renderContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		//ディスクリプタヒープを設定する。
		renderContext.SetDescriptorHeap(m_descriptorHeap);
		//描画
		renderContext.DrawIndexed(m_indexBuffer.GetCount());
	}


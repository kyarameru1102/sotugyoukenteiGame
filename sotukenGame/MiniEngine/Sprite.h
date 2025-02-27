#pragma once

#include "Indexbuffer.h"
#include "VertexBuffer.h"
#include "ConstantBuffer.h"

class Texture;

//スプライトに設定できる最大テクスチャ数。
const int MAX_TEXTURE = 16;	
//拡張SRVが設定されるレジスタの開始番号。
const int EXPAND_SRV_REG__START_NO = 10;

class IShaderResource;

template< class TExpandData > struct SpriteExpandDataInfo {
	TExpandData* m_expandData = nullptr;
	int m_expandDataSize = 0;
};
/// <summary>
/// スプライトの初期化データ。
/// </summary>
struct SpriteInitData {
	const char* m_ddsFilePath[MAX_TEXTURE]= {nullptr};		//DDSファイルのファイルパス。
	Texture* m_textures[MAX_TEXTURE] = { nullptr };			//使用するテクスチャ。DDSファイルのパスが指定されている場合は、このパラメータは無視されます。
	const char* m_vsEntryPointFunc = "VSMain";				//頂点シェーダーのエントリーポイント。
	const char* m_psEntryPoinFunc = "PSMain";				//ピクセルシェーダーのエントリーポイント。
	const char* m_fxFilePath = nullptr;						//.fxファイルのファイルパス。
	UINT m_width = 0;										//スプライトの幅。
	UINT m_height = 0;										//スプライトの高さ。
	void* m_expandConstantBuffer = nullptr;					//ユーザー拡張の定数バッファ
	int m_expandConstantBufferSize = 0;						//ユーザー拡張の定数バッファのサイズ。
	IShaderResource* m_expandShaderResoruceView = nullptr;	//ユーザー拡張のシェーダーリソース。
};
/// <summary>
/// スプライトクラス。
/// </summary>
class Sprite  {
public:
	static const Vector2	DEFAULT_PIVOT;					//!<ピボット。
	virtual ~Sprite();
	/// <summary>
	/// 初期化。
	/// </summary>
	/// <param name="initData">初期化データ</param>
	void Init(const SpriteInitData& initData);
	/// <summary>
	/// 更新。
	/// </summary>
	/// <param name="pos">座標</param>
	/// <param name="rot">回転</param>
	/// <param name="scale">拡大率</param>
	/// <param name="pivot">
	/// ピボット
	/// 0.5, 0.5で画像の中心が基点。
	/// 0.0, 0.0で画像の左下。
	/// 1.0, 1.0で画像の右上。
	/// UnityのuGUIに準拠。
	/// </param>
	void Update(const Vector3& pos, const Quaternion& rot, const Vector3& scale, const Vector2& pivot = DEFAULT_PIVOT);
	/// <summary>
	/// 描画。
	/// </summary>
	/// <param name="renderContext">レンダリングコンテキスト/param>
	void Draw(RenderContext& renderContext);

	/// <summary>
	/// 乗算カラーを設定
	/// </summary>
	/// <param name="mulColor">乗算カラー</param>
	void SetMulColor(const Vector4& mulColor)
	{
		m_mulColor = mulColor;
	}
	/// <summary>
	/// 乗算カラーを取得
	/// </summary>
	/// <returns>乗算カラー</returns>
	Vector4 GetMulColor() const
	{
		return m_mulColor;
	}
	/// <summary>
	/// a値の設定
	/// </summary>
	void SetAlpha(const float& alpha)
	{
		m_alpha = alpha;
	}
	/// <summary>
	/// a値を取得
	/// </summary>
	float GetAlpha() const
	{
		return m_alpha;
	}
	/// <summary>
	/// α値を変位させる
	/// </summary>
	/// <param name="delta">乗算αを変位させる量</param>
	void DeltaAlpha(const float& delta)
	{
		m_alpha += delta;
		//数値の境界チェック。
		if (m_alpha > 1.0f) {
			m_alpha = 1.0f;
		}
		else if (m_alpha < 0.0f) {
			m_alpha = 0.0f;
		}
	}
private:
	/// <summary>
	/// テクスチャを初期化。
	/// </summary>
	/// <param name="initData"></param>
	void InitTextures(const SpriteInitData& initData);
	/// <summary>
	/// シェーダーを初期化。
	/// </summary>
	/// <param name="initData"></param>
	void InitShader( const SpriteInitData& initData );
	/// <summary>
	/// ディスクリプタヒープを初期化。
	/// </summary>
	/// <param name="initData"></param>
	void InitDescriptorHeap(const SpriteInitData& initData);
	/// <summary>
	/// 頂点バッファとインデックスバッファを初期化。
	/// </summary>
	/// <param name="initData"></param>
	void InitVertexBufferAndIndexBuffer(const SpriteInitData& initData);
	/// <summary>
	/// パイプラインステートを初期化する。
	/// </summary>
	void InitPipelineState();
	/// <summary>
	/// 定数バッファを初期化。
	/// </summary>
	/// <param name="initData"></param>
	void InitConstantBuffer(const SpriteInitData& initData);

private:
	IndexBuffer m_indexBuffer;			//インデックスバッファ。
	VertexBuffer m_vertexBuffer;		//頂点バッファ。
	int m_numTexture = 0;				//テクスチャの枚数。
	Texture m_textures[MAX_TEXTURE];	//テクスチャ。
	Texture* m_textureExternal[MAX_TEXTURE] = {nullptr};	//外部から指定されたテクスチャ
	Vector3 m_position ;				//座標。
	Vector2 m_size;						//サイズ。
	Quaternion m_rotation ;			//回転。
	Matrix m_world;					//ワールド行列。
	Vector4					m_mulColor = { 1.0f, 1.0f, 1.0f, 1.0f };	//乗算カラー。
	float						m_alpha = 1.0f;							//スプライトのα値。

	struct LocalConstantBuffer {
		Matrix mvp;
		Vector4 mulColor;
		Vector4 screenParam;
	};
	LocalConstantBuffer m_constantBufferCPU;	//CPU側の定数バッファ。
	ConstantBuffer		m_constantBufferGPU;	//GPU側の定数バッファ。
	ConstantBuffer		m_userExpandConstantBufferGPU;	//ユーザー拡張の定数バッファ(GPU側)
	void* m_userExpandConstantBufferCPU = nullptr;		//ユーザー拡張の定数バッファ(CPU側)
	DescriptorHeap		m_descriptorHeap;		//ディスクリプタヒープ。
	RootSignature		m_rootSignature;		//ルートシグネチャ。
	PipelineState		m_pipelineState;		//パイプラインステート。
	Shader				m_vs;					//頂点シェーダー。
	Shader				m_ps;					//ピクセルシェーダー。
};
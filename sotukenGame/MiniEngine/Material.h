#pragma once

#include "tkFile/TkmFile.h"
/// <summary>
/// マテリアル。
/// </summary>
class Material {
public:
	/// <summary>
	/// tkmファイルのマテリアル情報から初期化する。
	/// </summary>
	/// <param name="tkmMat">tkmマテリアル</param>
	void InitFromTkmMaterila(
		const TkmFile::SMaterial& tkmMat,
		const wchar_t* fxFilePath,
		const char* vsEntryPointFunc,
		const char* psEntryPointFunc);
	/// <summary>
	/// レンダリングを開始するときに呼び出す関数。
	/// </summary>
	/// <param name="rc">レンダリングコンテキスト</param>
	/// <param name="hasSkin">スキンがあるかどうかのフラグ</param>
	void BeginRender(RenderContext& rc, int hasSkin, EnRenderMode renderMode);

	/// <summary>
	/// アルベドマップを取得。
	/// </summary>
	/// <returns></returns>
	Texture& GetAlbedoMap()
	{
		return m_albedoMap;
	}
	/// <summary>
	/// 法線マップを取得。
	/// </summary>
	/// <returns></returns>
	Texture& GetNormalMap()
	{
		return m_normalMap;
	}
	/// <summary>
	/// スペキュラマップを取得。
	/// </summary>
	/// <returns></returns>
	Texture& GetSpecularMap()
	{
		return m_specularMap;
	}
	/// <summary>
	/// 反射マップを取得。
	/// </summary>
	/// <returns></returns>
	Texture& GetReflectionMap()
	{
		return m_reflectionMap;
	}
	/// <summary>
	/// 屈折マップを取得。
	/// </summary>
	/// <returns></returns>
	Texture& GetRefractionMap()
	{
		return m_refractionMap;
	}
	/// <summary>
	/// 定数バッファを取得。
	/// </summary>
	/// <returns></returns>
	ConstantBuffer& GetConstantBuffer()
	{
		return m_constantBuffer;
	}
private:
	/// <summary>
	/// パイプラインステートの初期化。
	/// </summary>
	void InitPipelineState();
	/// <summary>
	/// シェーダーの初期化。
	/// </summary>
	/// <param name="fxFilePath">fxファイルのファイルパス</param>
	/// <param name="vsEntryPointFunc">頂点シェーダーのエントリーポイントの関数名</param>
	/// <param name="psEntryPointFunc">ピクセルシェーダーのエントリーポイントの関数名</param>
	void InitShaders(
		const wchar_t* fxFilePath,
		const char* vsEntryPointFunc,
		const char* psEntryPointFunc);
	/// <summary>
	/// テクスチャを初期化。
	/// </summary>
	/// <param name="tkmMat"></param>
	void InitTexture(const TkmFile::SMaterial& tkmMat);
private:
	/// <summary>
	/// マテリアルパラメータ。
	/// </summary>
	struct SMaterialParam {
		int hasNormalMap;	//法線マップを保持しているかどうかのフラグ。
		int hasSpecMap;		//スペキュラマップを保持しているかどうかのフラグ。
	};
	Texture	m_albedoMap;							//アルベドマップ。
	Texture	m_normalMap;							//法線マップ。
	Texture	m_specularMap;							//スペキュラマップ。
	Texture m_reflectionMap;						//リフレクションマップ。
	Texture m_refractionMap;						//屈折マップ。

	ConstantBuffer m_constantBuffer;				//定数バッファ。
	RootSignature m_rootSignature;					//ルートシグネチャ。
	PipelineState m_nonSkinModelPipelineState;		//スキンなしモデル用のパイプラインステート。
	PipelineState m_skinModelPipelineState;			//スキンありモデル用のパイプラインステート。
	PipelineState m_transSkinModelPipelineState;	//スキンありモデル用のパイプラインステート(半透明マテリアル)。
	PipelineState m_transNonSkinModelPipelineState;	//スキンなしモデル用のパイプラインステート(半透明マテリアル)。
	PipelineState m_nonSkinModelShadowMapPipelineState;	//シャドウマップ生成用のスキンなしモデル用のパイプラインステート。
	PipelineState m_skinModelShadowMapPipelineState;		//シャドウマップ生成用のスキンありモデル用のパイプラインステート。
	PipelineState m_nonSkinModelCascadeShadowMapPipelineState;	//カスケードシャドウマップ生成用のスキンなしモデル用のパイプラインステート。
	PipelineState m_skinModelCascadeShadowMapPipelineState;		//カスケードシャドウマップ生成用のスキンありモデル用のパイプラインステート。
	Shader m_vsNonSkinModel;						//スキンなしモデル用の頂点シェーダー。
	Shader m_vsSkinModel;							//スキンありモデル用の頂点シェーダー。
	Shader m_psModel;								//モデル用のピクセルシェーダー。
	Shader m_vsNonSkinShadowMap;				//シャドウマップ生成用のスキン無しモデルの頂点シェーダー
	Shader m_vsSkinShadowMap;					//シャドウマップ生成用のスキンありモデルの頂点シェーダー
	Shader m_vsNonSkinCascadeShadowMap;				//シャドウマップ生成用のスキン無しモデルの頂点シェーダー
	Shader m_vsSkinCascadeShadowMap;					//シャドウマップ生成用のスキンありモデルの頂点シェーダー
	Shader m_psShadowMap;						//シャドウマップ生成用のピクセルシェーダー

};



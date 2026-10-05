#pragma once
//=============================================================================
//
// シェーダー定数バッファ構造体たち [ConstantBufferStructs.h]
// Author : 
// DirectX HLSL に送る定数ちゃんを定義するファイルだよっ
// モデルのワールド行列やマテリアル、ライト、ボーンまで、全部ここから渡されるのっ！
// 小さなメモリにたくさんの思いを詰めて、GPUに届けるお手紙～
//
//=============================================================================
#include "main.h"

//*********************************************************
// マクロ定義
//*********************************************************
#define BONE_MAX			(512)   // ボーンの最大数
#define LIGHT_MAX			(5)		// ライトの最大数

//*********************************************************
// 構造体
//*********************************************************
struct WorldMatrixCBuffer
{
	XMMATRIX world;		// モデルのワールド変換行列
	XMMATRIX invWorld;	// 法線変換用の逆行列
};

// マテリアル用定数バッファ構造体
struct MATERIAL_CBUFFER
{
	XMFLOAT4	Ambient;					// 環境光
	XMFLOAT4	Diffuse;					// 拡散光	
	XMFLOAT4	Specular;					// 鏡面反射光
	XMFLOAT4	Emission;					// 放射光
	float		Shininess;					// 輝き度

	// それぞれのMapが有効かを 1 or 0 で指定
	int			noTexSampling;
	int			lightMapSampling;
	int			normalMapSampling;
	int			bumpMapSampling;
	int			opacityMapSampling;
	int			reflectMapSampling;
	int			translucencyMapSampling;
	//float		Dummy[1];				// 16byte境界用
};

// ライト用フラグ構造体
struct LIGHTFLAGS
{
	int			Type;		//ライトタイプ（enum LIGHT_TYPE）
	int         OnOff;		//ライトのオンorオフスイッチ
	int			Dummy[2];
};

// ライト用定数バッファ構造体
struct LIGHT_CBUFFER
{
	XMFLOAT4	Direction[LIGHT_MAX];		// ライトの方向
	XMFLOAT4	Position[LIGHT_MAX];		// ライトの位置
	XMFLOAT4	Diffuse[LIGHT_MAX];			// 拡散光の色
	XMFLOAT4	Ambient[LIGHT_MAX];			// 環境光の色
	XMFLOAT4	Attenuation[LIGHT_MAX];		// 減衰率
	LIGHTFLAGS	Flags[LIGHT_MAX];			// 種類やON/OFFなど
	XMFLOAT4X4 	LightViewProj[LIGHT_MAX];	// シャドウマップ用のViewProj行列
	int			Enable;						// ライティング有効・無効フラグ
	int			Dummy[3];					// 16byte境界用
};

// フォグ用定数バッファ構造体
struct FOG_CBUFFER
{
	XMFLOAT4	Fog;					// フォグ量
	XMFLOAT4	FogColor;				// フォグの色
	int			Enable;					// フォグ有効・無効フラグ
	float		Dummy[3];				// 16byte境界用
};

struct LIGHTMODE_CBUFFER
{
	int			mode;
	int			padding[3];
};

struct BoneMatricesCBuffer
{
	XMMATRIX bones[BONE_MAX];
};

struct RenderProgressCBuffer
{
	float progress = 0.0f;
	int isRandomFade = true;
	XMFLOAT2 padding{};
};
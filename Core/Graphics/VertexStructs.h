#pragma once
//=============================================================================
//
// 頂点構造体まとめ [VertexStructs.h]
// Author : 
// モデル・UI・エフェクト・インスタンス化など、いろんな用途の頂点フォーマットがここに集合中っ
// DirectXに渡すための構造を定義して、InputLayoutで使えるようにしているよ～
// 
//=============================================================================
#include "main.h"

//*********************************************************
// 構造体
//*********************************************************

// 通常の3Dモデルちゃん用の頂点構造体
struct VERTEX_3D
{
	XMFLOAT3	Position;	// 座標
	XMFLOAT3	Normal;		// 法線
	XMFLOAT4	Diffuse;	// 頂点カラー
	XMFLOAT2	TexCoord;	// テクスチャ座標
	XMFLOAT3	Tangent;	// 接ベクトル（法線マッピング用）

	VERTEX_3D(void)
	{
		Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
		Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
		Tangent = XMFLOAT3(0.0f, 0.0f, 0.0f);
		Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		TexCoord = XMFLOAT2(0.0f, 0.0f);
	}

	VERTEX_3D(XMFLOAT3 pos, XMFLOAT3 norm, XMFLOAT3 tangent, XMFLOAT4 dif, XMFLOAT2 tex)
	{
		Position = pos;
		Normal = norm;
		Tangent = tangent;
		Diffuse = dif;
		TexCoord = tex;
	}
};

// ボーンアニメーション付きモデルのためのスキン付き頂点構造体
struct SKINNED_VERTEX_3D
{
	XMFLOAT3	Position;		// 座標
	XMFLOAT3	Normal;			// 法線
	XMFLOAT3	Tangent;		// 接ベクトル（法線マッピング用）
	XMFLOAT3	Bitangent;		// 法線とタンジェントから求まる補助ベクトル
	XMFLOAT4	Diffuse;		// 頂点カラー
	XMFLOAT2	TexCoord;		// テクスチャ座標
	XMFLOAT4	Weights;		// 各ボーンの重み（最大4つまで対応）
	XMFLOAT4	BoneIndices;	// 対応するボーンのインデックス番号ちゃん

	SKINNED_VERTEX_3D(void)
	{
		Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
		Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
		Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		TexCoord = XMFLOAT2(0.0f, 0.0f);
		Tangent = XMFLOAT3(0.0f, 0.0f, 0.0f);
		Bitangent = XMFLOAT3(0.0f, 0.0f, 0.0f);
		//for (int i = 0; i < MAX_BONE_INDICES; i++)
		//{
		//	Weights[i] = 0.0f;
		//	BoneIndices[i] = 0;
		//}
		Weights = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		BoneIndices = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	SKINNED_VERTEX_3D(XMFLOAT3 pos, XMFLOAT3 norm, XMFLOAT3 tangent, XMFLOAT4 dif, XMFLOAT2 tex)
	{
		Position = pos;
		Normal = norm;
		Tangent = tangent;
		Diffuse = dif;
		TexCoord = tex;
		//for (int i = 0; i < MAX_BONE_INDICES; i++)
		//{
		//	Weights[i] = 0.0f;
		//	BoneIndices[i] = 0;
		//}
		Bitangent = XMFLOAT3(0.0f, 0.0f, 0.0f);
		Weights = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		BoneIndices = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	}
};

// UI描画用の頂点構造体
struct UIVertex
{
	XMFLOAT2 Position;	// 画面上の座標
	XMFLOAT2 TexCoord;	// テクスチャ座標
	XMFLOAT4 Color;		// 頂点カラー（RGBA）
};


// VFX描画用の頂点構造体
struct VFXVertex
{
	XMFLOAT3 position;	// 位置 (ワールド座標)
	XMFLOAT2 uv;		// テクスチャ座標
	XMFLOAT4 color;		// 頂点カラー（RGBA）
};

// インスタンス化されたモデル用の追加情報構造体
struct InstanceData
{
	XMFLOAT3 OffsetPosition;		// インスタンス位置オフセット (ワールド座標)
	XMFLOAT4 Rotation;				// 四元数での回転
	XMFLOAT4 initialBillboardRot;	// 初期ビルボード回転角度
	float Scale;					// 拡縮
	float Type;						// 種類の識別番号

	InstanceData()
	{
		OffsetPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
		Rotation = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		initialBillboardRot = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		Scale = 1.0f;
		Type = 0.0f;
	}

	InstanceData(XMFLOAT3 offset, XMFLOAT4 rot, XMFLOAT4 billboardRot, float scl, float type) :
		OffsetPosition(offset), Rotation(rot), initialBillboardRot(billboardRot), Scale(scl), Type(type) {
	}
};

// 流体シミュレーション用の頂点構造体
struct FluidSurfaceVertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT2 texcoord;
};

// マーチングキューブ用の頂点構造体
struct MarchingCubesVertex
{
	XMFLOAT3 position; // 頂点位置
	XMFLOAT3 normal;   // 法線ベクトル
};

struct ProjectilePreviewVertex 
{
	XMFLOAT3 pos;
	XMFLOAT3 normal;
};
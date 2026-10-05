#pragma once
//=============================================================================
//
// パーティクル内部構造体定義 [ParticleStructs.h]
// Author : 
// Compute Shaderで扱うパーティクル個別データを定義するヘッダにゃん
// Billboard系パーティクル用（通常 / Flipbook）に対応っ！ぷにぷに
//
//=============================================================================
#include "Effects/IEffectRenderer.h"

//*********************************************************
// 構造体
//*********************************************************

// --- BillboardSimple ---
struct BillboardSimpleParticle
{
    XMFLOAT3 position;
    XMFLOAT3 velocity;
    float size;
    float life;
    float lifeRemaining;
    float rotation;
    XMFLOAT4 color;
    XMFLOAT4 startColor;
    XMFLOAT4 endColor;
};

// --- BillboardFlipbook ---
struct BillboardFlipbookParticle
{
    XMFLOAT3 position;              // 現在位置
    XMFLOAT3 velocity;              // 速度
    float size;                     // サイズ
    float life;
    float lifeRemaining;            // 残り寿命（秒）
    float rotation;                 // 回転角（ラジアン）
    XMFLOAT4 color;
    XMFLOAT4 startColor;
    XMFLOAT4 endColor;
    float frameIndex;               // アニメーションの現在フレーム（float、小数対応）
    float frameSpeed;               // 1秒あたり再生速度（補間対応用
};

// 水体シミュレーション用の粒子構造体
// Position Based Fluids (PBF) に基づいて、物理的な水挙動を表現する
struct WaterFluidParticle
{
	UINT     particleID;           // 粒子ID（0から始まるインデックス）
    XMFLOAT3 position;             // 現在位置
	XMFLOAT3 prevPosition;         // 前フレーム位置
    float    density;              // 密度（周囲の粒子から算出）

    XMFLOAT3 velocity;             // 速度
    float    lambda;               // 拘束条件解（位置補正のための係数）
	float    pressure;             // 圧力（密度から算出される）

    XMFLOAT3 predictedPosition;    // 予測位置（次の位置、拘束前）
    float    lifeRemaining;        // 寿命（将来拡張用、今は1で固定でも可）

    XMFLOAT4 color;                // デバッグ用カラー（密度に応じた可視化など）

    float size;                    // サイズ
};

// 粒子構造体
struct FallingParticle
{
    UINT particleID;                // 粒子ID（0から始まるインデックス）
    XMFLOAT3 position;              // 現在位置
    XMFLOAT3 prevPosition;          // 前フレーム位置

    XMFLOAT3 velocity;              // 速度

    XMFLOAT3 predictedPosition;     // 予測位置（次の位置、拘束前）
    float lifeRemaining;            // 寿命（将来拡張用、今は1で固定でも可）

    XMFLOAT4 color;                 // デバッグ用カラー（密度に応じた可視化など）

    float size; // サイズ
};
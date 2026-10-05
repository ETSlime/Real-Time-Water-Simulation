#pragma once
//=============================================================================
//
// 古の.obj書を読み解く司書系ローダーちゃん　[OBJLoader.h]
// Author : 
// 拡張子.objのモデルデータを読み取り、マテリアルやテクスチャまで丁寧に取り出してくれる専門職ちゃん
//「わたし、こういう昔のフォーマットを扱うの…得意なんです」って微笑んでくれる子なの～っ！
//
//=============================================================================
#include "main.h"
#include "Utility/SingletonBase.h"
#include "Model/Model.h"

class OBJLoader : public SingletonBase<OBJLoader>
{
public:
	// モデル読込
	bool LoadObjModel(const char* FileName, StaticModelData* Model, AxisFlip flip = AxisFlip::None);
private:
	//マテリアル読み込み
	void LoadMaterial(char* FileName, MODEL_MATERIAL** MaterialArray, unsigned int* MaterialNum);
	// テクスチャ名読み込み
	void LoadTextureName(char* FileName, FILE* fp, MODEL_MATERIAL* Material, int mc, TextureType type);

	bool m_loadTangent = false;
};
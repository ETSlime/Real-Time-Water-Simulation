#pragma once
//=============================================================================
//
// [SaveSystem.h]
// Author : 
// 
//=============================================================================
#include "main.h"
#include "Scene/Player.h"
#include "Scene/Ground.h"

struct SaveData
{
	int	seedCount;
	int bulletCount;
	int keyCount;
	bool hasBullet;
	XMFLOAT3 pos;
	SceneID currentSceneID;
};

class SaveSystem : public SingletonBase<SaveSystem>
{
public:
	bool SaveGame(void);
	bool LoadGame(SaveData& saveData);
	bool CheckSaveDataExists(void);

private:
	void GetSaveData(SaveData& saveData);

	// 任意型Tの値を「ネイティブメモリ順」で16進テキストとして書き出す
	template <typename T>
	bool WriteHex(FILE* fp, const T& value);

	// 任意型Tの値を「ネイティブメモリ順」で16進テキストとして読み込む
	template <typename T>
	bool ReadHex(FILE* fp, T& value);

	// 2桁の16進で1バイトを書き出すヘルパー（大文字）
	inline bool WriteOneHexByte(FILE* fp, unsigned char b) 
	{
		// 例: 0x0A -> "0A"
		return fprintf(fp, "%02X", static_cast<unsigned int>(b)) == 2;
	}

	// 2桁の16進で1バイト読み込むヘルパー
	bool ReadNextHexByte(FILE* fp, unsigned char& outByte);
};

template<typename T>
inline bool SaveSystem::WriteHex(FILE* fp, const T& value)
{
	const unsigned char* byteData = reinterpret_cast<const unsigned char*>(&value);
	for (size_t i = 0; i < sizeof(T); ++i) 
	{
		if (!WriteOneHexByte(fp, byteData[i])) return false;
	}

	return true;
}

template<typename T>
inline bool SaveSystem::ReadHex(FILE* fp, T& value)
{
	unsigned char* byteData = reinterpret_cast<unsigned char*>(&value);

	for (size_t i = 0; i < sizeof(T); ++i) 
	{
		unsigned char b = 0;
		if (!ReadNextHexByte(fp, b)) return false;
		byteData[i] = b;
	}
	return true;
}

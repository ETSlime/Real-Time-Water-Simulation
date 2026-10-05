//=============================================================================
//
// モデルキャッシュの読み書き [ModelCacheLoader.h]
// Author :
// モデルデータを .bin 形式で保存＆読み込みして、高速ロードを実現する便利クラスですっ
// ファイル操作も安全にラップして、読み込み中にエラーしないように守ってくれるえらい子なのです
//
//=============================================================================
#pragma once
#include "main.h"
#include "Core/Graphics/Renderer.h"
#include "Utility/SimpleArray.h"

struct StaticModelData;

class ModelCacheLoader 
{
public:

    static inline bool SafeReadFile(HANDLE hFile, void* dest, DWORD size) 
    {
        DWORD bytesRead;
        return ReadFile(hFile, dest, size, &bytesRead, nullptr);
    }

    // 書き込み用の安全ラッパー関数
    static bool SafeWriteFile(HANDLE hFile, const void* src, DWORD size) 
    {
        DWORD bytesWritten = 0;
        // WriteFile が失敗した場合や、書き込んだバイト数が一致しない場合は false を返す
        return WriteFile(hFile, src, size, &bytesWritten, nullptr) && bytesWritten == size;
    }

    static bool LoadFromCache(const char* cachePath, StaticModelData* modelData);
    static bool SaveToCache(const char* cachePath, const StaticModelData* modelData);

    // .obj パスから .bin パスに変換（char* を使う）
    static void ConvertToBinPath(const char* objPath, char* outBinPath, size_t maxLength);
};
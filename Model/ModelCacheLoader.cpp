//=============================================================================
//
// ModelCacheLoader処理 [ModelCacheLoader.cpp]
// Author :
//
//=============================================================================
#include "Model/ModelCacheLoader.h"
#include "Model/Model.h"

bool ModelCacheLoader::LoadFromCache(const char* cachePath, StaticModelData* modelData)
{
    char binPath[MAX_PATH];
    ConvertToBinPath(cachePath, binPath, MAX_PATH);

    HANDLE file = CreateFileA(binPath, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    DWORD bytesRead = 0;

    // ヘッダー読み込み（頂点数・インデックス数・サブセット数）
    if (!SafeReadFile(file, &modelData->VertexNum, sizeof(unsigned int)) ||
        !SafeReadFile(file, &modelData->IndexNum, sizeof(unsigned int)) ||
        !SafeReadFile(file, &modelData->SubsetNum, sizeof(unsigned int)) ||
        !SafeReadFile(file, &modelData->boundingBox, sizeof(BOUNDING_BOX))) 
    {
        CloseHandle(file);
        return false;
    }

    // 頂点配列読み込み
    modelData->VertexArray = new VERTEX_3D[modelData->VertexNum];
    if (!SafeReadFile(file, modelData->VertexArray, sizeof(VERTEX_3D) * modelData->VertexNum)) 
    {
        CloseHandle(file);
        return false;
    }

    // インデックス配列読み込み
    modelData->IndexArray = new unsigned int[modelData->IndexNum];
    if (!SafeReadFile(file, modelData->IndexArray, sizeof(unsigned int) * modelData->IndexNum)) 
    {
        CloseHandle(file);
        return false;
    }

    // サブセット読み込み（テクスチャパス含むが、SRVは含まない）
    modelData->SubsetArray = new SUBSET[modelData->SubsetNum];
    if (!SafeReadFile(file, modelData->SubsetArray, sizeof(SUBSET) * modelData->SubsetNum)) 
    {
        CloseHandle(file);
        return false;
    }

    CloseHandle(file);
    return true;
}

bool ModelCacheLoader::SaveToCache(const char* cachePath, const StaticModelData* modelData)
{
    char binPath[MAX_PATH];
    ConvertToBinPath(cachePath, binPath, MAX_PATH);

    HANDLE file = CreateFileA(binPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    // ヘッダー書き込み（頂点数・インデックス数・サブセット数・バウンディングボックス）
    if (!SafeWriteFile(file, &modelData->VertexNum, sizeof(unsigned int)) ||
        !SafeWriteFile(file, &modelData->IndexNum, sizeof(unsigned int)) ||
        !SafeWriteFile(file, &modelData->SubsetNum, sizeof(unsigned int)) ||
        !SafeWriteFile(file, &modelData->boundingBox, sizeof(BOUNDING_BOX))) 
    {
        CloseHandle(file);
        return false;
    }

    // 頂点配列書き込み
    if (!SafeWriteFile(file, modelData->VertexArray, sizeof(VERTEX_3D) * modelData->VertexNum)) 
    {
        CloseHandle(file);
        return false;
    }

    // インデックス配列書き込み
    if (!SafeWriteFile(file, modelData->IndexArray, sizeof(unsigned int) * modelData->IndexNum)) 
    {
        CloseHandle(file);
        return false;
    }

    // サブセット書き込み
    if (!SafeWriteFile(file, modelData->SubsetArray, sizeof(SUBSET) * modelData->SubsetNum))
    {
        CloseHandle(file);
        return false;
    }

    CloseHandle(file);
    return true;
}

void ModelCacheLoader::ConvertToBinPath(const char* objPath, char* outBinPath, size_t maxLength)
{
    size_t len = strlen(objPath);
    size_t dotPos = len;
    for (size_t i = len; i > 0; --i) {
        if (objPath[i - 1] == '.') {
            dotPos = i - 1;
            break;
        }
    }

    size_t copyLen = (dotPos < maxLength - 5) ? dotPos : maxLength - 5;
    memcpy(outBinPath, objPath, copyLen);
    outBinPath[copyLen] = '\0';
    strcat_s(outBinPath, maxLength, ".bin");
}

//=============================================================================
//
// シェーダーファイルの変更検知とホットリロード機構 [ShaderReloader.h]
// Author : 
// ファイル更新時刻・バイナリハッシュを比較し、エントリ単位での
// 差分検出と動的再生成（ホットリロード）をサポートするユーティリティ
// 
//=============================================================================
#pragma once
#include "Utility/HashMap.h"
#include "Core/Shader/ShaderLoader.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************

//==============================================================
// REGISTER_SHADER_RELOAD_ENTRY マクロ定義
// - shaderType は D3D11 シェーダークラス名（Vertex, Pixel, Compute, Geometry）
// - ptr は ID3D11XXXShader* 型の変数
// - target は "vs_5_0" など
//==============================================================
#define REGISTER_SHADER_RELOAD_ENTRY(manager, shaderGroup, shaderSetID, shaderType, computePass, shaderTypeName, path, entry, target, ptr)  \
    manager.RegisterShaderReloadEntry(                                                                  \
        path,                                                                                           \
        entry,                                                                                          \
        target,                                                                                         \
        reinterpret_cast<void**>(ptr),                                                                  \
        [](ID3D11Device* dev, const void* data, SIZE_T size, void** out) -> HRESULT {                   \
            return dev->Create##shaderTypeName##Shader(data, size, nullptr,                             \
                reinterpret_cast<ID3D11##shaderTypeName##Shader**>(out));                               \
        },                                                                                              \
        shaderGroup,                                                                                    \
        TO_UINT64(shaderSetID),                                                                         \
        shaderType,                                                                                     \
        computePass)

enum class ShaderGroup : uint8_t
{
    Default,
    Particle,
    Shadow,
    Compute,
    ParticleCompute
};

enum class ShaderType : uint8_t
{
    VS, GS, PS, PS_ALPHA, CS
};

//*********************************************************
// 構造体
//*********************************************************

// シェーダー再生成関数ポインタ型
typedef HRESULT(*ShaderCreateFunc)(ID3D11Device* device, const void* bytecode, SIZE_T size, void** outShader);

struct ShaderReloadEntry
{
    const char* path;                   // シェーダーファイル
    const char* entry;                  // エントリーポイント
    const char* target;                 // コンパイルターゲット (vs_5_0 / ps_5_0)
    uint64_t shaderSetID;
    ShaderGroup shaderGroup;
    ShaderType  shaderType;
    ComputePassType computePass;
    void** outputShaderPtr;             // 出力先のシェーダーポインタ (void**)
    ShaderCreateFunc createShaderFunc;  // シェーダー生成関数
};

struct ShaderSourceKey
{
    // path + entry をキーに
    const char* path;
    const char* entry;

    bool operator==(const ShaderSourceKey& other) const
    {
        return strcmp(path, other.path) == 0 && strcmp(entry, other.entry) == 0;
    }
};

struct ShaderSourceKeyHash
{
    unsigned int operator()(const ShaderSourceKey& key) const
    {
        // DJB2 ハッシュ関数を使って path と entry を合成ハッシュ
        unsigned long hash = 5381;

        const char* p = key.path;
        while (p && *p)
        {
            hash = ((hash << 5) + hash) + *p++;
        }

        const char* e = key.entry;
        while (e && *e)
        {
            hash = ((hash << 5) + hash) + *e++;
        }

        return static_cast<unsigned int>(hash);
    }
};

// 等価比較演算子
struct ShaderSourceKeyEquals
{
    bool operator()(const ShaderSourceKey& a, const ShaderSourceKey& b) const
    {
        return strcmp(a.path, b.path) == 0 && strcmp(a.entry, b.entry) == 0;
    }
};


struct FileState
{
    FILETIME lastWriteTime;
    uint64_t contentHash;
};

class ShaderReloader
{
public:
    // ファイルを監視対象に追加する
    void WatchFile(const char* path);

    // ファイルが変更されたかをチェックする
    bool HasFileChanged(const char* path);

    //void AdvanceFrame(void);

    bool HasShaderContentChanged(const char* path, const char* entry, const void* blob, size_t size);

private:

    //bool GetFileHashAndTime(const char* path, FILETIME& outTime, uint64_t& outHash);
    uint64_t CalculateBlobHash(const void* data, size_t size);
    uint64_t CalculateFileHash(const char* path);

    // ファイルパスと最終更新時刻のマップ
    HashMap<const char*, FileState, CharPtrHash, CharPtrEquals> m_fileStateMap;
    // entryレベルのblob比較
    HashMap<ShaderSourceKey, uint64_t, ShaderSourceKeyHash, ShaderSourceKeyEquals> m_blobHashMap;
    // フレームごとの fallback チェック用
    int m_frameCount = 0;
    static constexpr int kHashFallbackInterval = 30; // 30フレームごと  fallback hash チェック
};

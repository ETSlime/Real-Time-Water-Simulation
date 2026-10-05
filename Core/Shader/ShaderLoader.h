#pragma once
//=============================================================================
//
// HLSL シェーダー読み込み・入力レイアウト生成ユーティリティ [ShaderLoader.h]
// Author : 
// 頂点・ピクセル・ジオメトリ・コンピュートシェーダーを統一的に読み込み、
// 反射情報による InputLayout の自動生成およびキャッシュ付きコンパイルに対応
// 
//=============================================================================
#include "main.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
// HLSL シェーダーモデルのバージョンを統一管理するマクロ
#define SHADER_MODEL_VS "vs_4_0"
#define SHADER_MODEL_PS "ps_4_0"
#define SHADER_MODEL_GS "gs_4_0"
#define SHADER_MODEL_CS "cs_5_0" // AppendStructuredBufferは5.0以上が必要

// 着色器 ID を一括管理する列挙型
enum class ShaderSetID : uint64_t
{
    None,
    StaticModel,
    SkinnedModel,
    Instanced_Grass,
    Instanced_Tree,
    Decal,
    Skybox,
    VFX,
    WATER,
    SOFT_BODY,
    LakeSurface,
    FireEffect,
    FireBallEffect,
    SmokeEffect,
    FluidSurface,
    FluidMarchingCube,
    FluidMarchingCubeGPU,
    ProjectilePreview,
    Laser,
    UI,
    Debug,
};

enum class ParticleShaderGroup : uint64_t
{
    None,
    BillboardSimple,
    BillboardFlipbook,
    BillboardSoftParticle,
    BillboardFlipbookSoft,
    WaterFluid,
    Falling,
    Count,
};

enum class ParticleComputeGroup : uint64_t
{
    None,
    BasicBillboard,          // BillboardSimple
    FlipbookAnimated,        // BillboardFlipbook
    FlipbookWithSoft,        // BillboardFlipbookSoft
    WaterFluid,
    Falling,
};

// コンピュートパスの種類を管理する列挙型
enum class ComputePassType : uint64_t
{
    None,
    Update,
    Emit,
    Sort,
    Blur,
    SPH_BuildGrid,
    SPH_Simulate_Dense_Grid,
	SPH_Simulate_Sparse_Grid,
    MarchingCubes,
    GenerateScalarField,
    MCVoxelLabelPropagation,
    MCVoxelClusterPartition,
    MCVoxelPrefixSumLocal,
    MCVoxelPrefixSumGlobal,
    MCVoxelPrefixSumAddOffset,
	Particle_Thickness,
    PreIntegration,
};

// 頂点レイアウト ID（InputLayout を管理するため）
enum class VertexLayoutID : uint64_t
{
    Static,
    Skinned,
    Instanced,
    Skybox,
    VFX,
    Laser,
    ProjectilePreview,
    FluidSurface,
    UI,
    Decal,
    Debug,

    //VertexIDOnly
    ParticleSimple,
    ParticleFlipbook,
    FireEffect,
    SmokeEffect,
    FireBallEffect,
    SoftBody,
};


//*********************************************************
// 構造体
//*********************************************************
struct ShaderSet
{
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    ID3D11GeometryShader* gs = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;

    VertexLayoutID layoutID = VertexLayoutID::Static;
};

struct ShadowShaderSet
{
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    ID3D11PixelShader* alphaPs = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;

    VertexLayoutID layoutID = VertexLayoutID::Static;
};

struct ComputeShaderSet 
{
    ID3D11ComputeShader* cs = nullptr;
    const char* path = nullptr;
    const char* entry = nullptr;

    ComputePassType pass = ComputePassType::None;
    uint64_t shaderSetID = 0;
};

struct ComputeShaderSetKey
{
    uint64_t shaderID;
    ComputePassType pass;
};

struct ShaderSetKeyHash
{
    size_t operator()(const ComputeShaderSetKey& k) const
    {
        return (static_cast<size_t>(k.shaderID) << 8) ^ static_cast<size_t>(k.pass);
    }
};

struct EqualShaderSetKey
{
    bool operator()(const ComputeShaderSetKey& a, const ComputeShaderSetKey& b) const
    {
        return a.shaderID == b.shaderID && a.pass == b.pass;
    }
};


class ShaderLoader
{
public:
    // Shader（VS + PS + InputLayout）
    static bool LoadShaderSet(ID3D11Device* device,
        const char* vsPath, const char* vsEntry,
        const char* psPath, const char* psEntry,
        const char* gsPath, const char* gsEntry,
        const D3D11_INPUT_ELEMENT_DESC* layoutDesc,
        UINT numElements,
        ShaderSet& outShaderSet);
    static bool LoadShaderSetWithoutLayoutCreation(ID3D11Device* device,
        const char* vsPath, const char* vsEntry,
        const char* psPath, const char* psEntry,
        const char* gsPath, const char* gsEntry,
        ShaderSet& outShaderSet);

    // ShadowShaderSet（Alpha テスト含む）
    static bool LoadShadowShaderSet(ID3D11Device* device,
        const char* vsPath, const char* psPath, const char* psAlphaPath,
        const char* vsEntry, const char* psEntry, const char* psAlphaEntry,
        const D3D11_INPUT_ELEMENT_DESC* layoutDesc,
        UINT numElements,
        ShadowShaderSet& outShaderSet);
    static bool LoadShadowShaderSetWithoutLayoutCreation(ID3D11Device* device,
        const char* vsPath, const char* psPath, const char* psAlphaPath,
        const char* vsEntry, const char* psEntry, const char* psAlphaEntry,
        ShadowShaderSet& outShaderSet);

    // ComputerShaderSet
    static bool LoadComputerShaderSet(ID3D11Device* device, ComputeShaderSet& outShaderSet);

    // D3DX11CompileFromFile をそのまま使う（非推奨・キャッシュあり）
    static bool CompileShaderFromFileLegacy(const char* fileName, const char* entryPoint, const char* target, ID3DBlob** blobOut);
    // 内部で fread → D3DCompile（最新版・キャッシュ対策済み）
    static bool CompileShaderFromFile(const char* fileName, const char* entryPoint, const char* target, ID3DBlob** blobOut);

    // 空のピクセルシェーダーを読み込む関数
    static bool LoadEmptyPixelShader(ID3D11Device* device, ID3D11PixelShader** outPixelShader, const char* fileName, const char* entryPoint);

private:

    static bool CreateInputLayoutFromReflection(ID3D11Device* device, ID3DBlob* vsBlob, ID3D11InputLayout** outLayout);

    static bool CompileAndCreateVertexShader(ID3D11Device* device, const char* path, const char* entry, ID3D11VertexShader** outVS, ID3DBlob** outVSBlob);
    static bool CompileAndCreatePixelShader(ID3D11Device* device, const char* path, const char* entry, ID3D11PixelShader** outPS);
    static bool CompileAndCreateGeometryShader(ID3D11Device* device, const char* path, const char* entry, ID3D11GeometryShader** outGS);
    static bool CompileAndCreateComputeShader(ID3D11Device* device, const char* path, const char* entry, ID3D11ComputeShader** outCS);
};
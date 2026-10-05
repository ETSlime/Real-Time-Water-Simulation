//=============================================================================
//
// ShaderLoader処理 [ShaderLoader.cpp]
// Author : 
//
//=============================================================================
#include "Core/Shader/ShaderLoader.h"
#include "Core/Shader/ShaderInclude.h"
#include "Utility/SimpleArray.h"


bool ShaderLoader::LoadShaderSet(ID3D11Device* device,
    const char* vsPath, const char* vsEntry,
    const char* psPath, const char* psEntry,
    const char* gsPath, const char* gsEntry,
    const D3D11_INPUT_ELEMENT_DESC* layoutDesc,
    UINT numElements,
    ShaderSet& outShaderSet)
{
    ID3DBlob* vsBlob = nullptr;
    if (!CompileAndCreateVertexShader(device, vsPath, vsEntry, &outShaderSet.vs, &vsBlob))
        return false;

    if (layoutDesc)
    {
        device->CreateInputLayout(layoutDesc, numElements, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &outShaderSet.inputLayout);
    }
    else
    {
        if (!CreateInputLayoutFromReflection(device, vsBlob, &outShaderSet.inputLayout))
            return false;
    }

    vsBlob->Release();

    if (!CompileAndCreatePixelShader(device, psPath, psEntry, &outShaderSet.ps))
        return false;

    if (gsPath && !CompileAndCreateGeometryShader(device, gsPath, gsEntry, &outShaderSet.gs))
        return false;

    return true;
}


bool ShaderLoader::LoadShaderSetWithoutLayoutCreation(ID3D11Device* device,
    const char* vsPath, const char* vsEntry, 
    const char* psPath, const char* psEntry,
    const char* gsPath, const char* gsEntry,
    ShaderSet& outShaderSet)
{
    ID3DBlob* vsBlob = nullptr;
    if (!CompileAndCreateVertexShader(device, vsPath, vsEntry, &outShaderSet.vs, &vsBlob))
        return false;

    vsBlob->Release();

    if (!CompileAndCreatePixelShader(device, psPath, psEntry, &outShaderSet.ps))
        return false;

    if (gsPath && !CompileAndCreateGeometryShader(device, gsPath, gsEntry, &outShaderSet.gs))
        return false;

    return true;
}

bool ShaderLoader::LoadShadowShaderSet(ID3D11Device* device,
    const char* vsPath, const char* psPath, const char* psAlphaPath,
    const char* vsEntry, const char* psEntry, const char* psAlphaEntry,
    const D3D11_INPUT_ELEMENT_DESC* layoutDesc,
    UINT numElements,
    ShadowShaderSet& outShaderSet)
{
    ID3DBlob* vsBlob = nullptr;
    if (!CompileAndCreateVertexShader(device, vsPath, vsEntry, &outShaderSet.vs, &vsBlob))
        return false;

    if (layoutDesc)
        device->CreateInputLayout(layoutDesc, numElements, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &outShaderSet.inputLayout);
    else
        CreateInputLayoutFromReflection(device, vsBlob, &outShaderSet.inputLayout);

    vsBlob->Release();

    if (psPath && !CompileAndCreatePixelShader(device, psPath, psEntry, &outShaderSet.ps))
        return false;

    if (psAlphaPath && !CompileAndCreatePixelShader(device, psAlphaPath, psAlphaEntry, &outShaderSet.alphaPs))
        return false;

    return true;
}


bool ShaderLoader::LoadShadowShaderSetWithoutLayoutCreation(ID3D11Device* device,
    const char* vsPath, const char* psPath, const char* psAlphaPath,
    const char* vsEntry, const char* psEntry, const char* psAlphaEntry,
    ShadowShaderSet& outShaderSet)
{
    ID3DBlob* vsBlob = nullptr;
    if (!CompileAndCreateVertexShader(device, vsPath, vsEntry, &outShaderSet.vs, &vsBlob))
        return false;

    vsBlob->Release();

    if (psPath && !CompileAndCreatePixelShader(device, psPath, psEntry, &outShaderSet.ps))
        return false;

    if (psAlphaPath && !CompileAndCreatePixelShader(device, psAlphaPath, psAlphaEntry, &outShaderSet.alphaPs))
        return false;

    return true;
}

bool ShaderLoader::LoadComputerShaderSet(ID3D11Device* device, ComputeShaderSet& outShaderSet)
{
    if (outShaderSet.path && !CompileAndCreateComputeShader(device, outShaderSet.path, outShaderSet.entry, &outShaderSet.cs))
        return false;

    return true;
}

bool ShaderLoader::CompileShaderFromFileLegacy(const char* fileName, const char* entryPoint, const char* target, ID3DBlob** blobOut)
{
    DWORD shaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#if defined( _DEBUG )
    shaderFlags |= D3DCOMPILE_DEBUG;
#endif

    if (entryPoint == nullptr)
        return false;

    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = D3DX11CompileFromFile(
        fileName,
        nullptr, nullptr,
        entryPoint, target,
        shaderFlags, 0,
        nullptr,
        blobOut, &errorBlob, nullptr);

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }

    assert(SUCCEEDED(hr));

    return true;
}

bool ShaderLoader::CompileShaderFromFile(const char* fileName, const char* entryPoint, const char* target, ID3DBlob** blobOut)
{
    // ファイルをバイナリで開く
    FILE* file = nullptr;
    if (fopen_s(&file, fileName, "rb") != 0 || file == nullptr)
        return false;

    // ファイルサイズを取得
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size <= 0)
    {
        fclose(file);
        return false;
    }

    // メモリ確保
    char* buffer = new char[size];
    if (fread(buffer, 1, size, file) != size)
    {
        delete[] buffer;
        fclose(file);
        return false;
    }

    fclose(file);

    // フラグ設定（デバッグ時は最適化オフ）
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pErrorBlob = nullptr;
	ShaderInclude includeHandler; // シェーダーインクルードハンドラー
    // D3DCompile を使用してシェーダーをコンパイル
    HRESULT hr = D3DCompile(
        buffer, size,
        fileName,                           // ソース名（警告/エラー表示用）
        nullptr,                            // defines
		&includeHandler,                    // インクルードハンドラー
        entryPoint, target,
        flags, 0,
        blobOut, &pErrorBlob
    );

    char* error = nullptr;
    if (!SUCCEEDED(hr))
        error = (char*)pErrorBlob->GetBufferPointer();

    assert(SUCCEEDED(hr));

    delete[] buffer;
    return SUCCEEDED(hr);
}

bool ShaderLoader::LoadEmptyPixelShader(ID3D11Device* device, ID3D11PixelShader** outPixelShader, const char* fileName, const char* entryPoint)
{
    if (!device || !outPixelShader) return false;

    ID3DBlob* blob = nullptr;
    HRESULT hr = CompileShaderFromFile(fileName, entryPoint, SHADER_MODEL_PS, &blob);
    if (FAILED(hr) || !blob) 
        return false;

    hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, outPixelShader);
    blob->Release();

    return SUCCEEDED(hr);
}

bool ShaderLoader::CreateInputLayoutFromReflection(ID3D11Device* device, ID3DBlob* vsBlob, ID3D11InputLayout** outLayout)
{
    // シェーダーリフレクションのインターフェースを取得
    ID3D11ShaderReflection* reflector = nullptr;
    HRESULT hr = D3DReflect(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&reflector);
    if (FAILED(hr)) return false;

    // シェーダーの詳細情報を取得
    D3D11_SHADER_DESC shaderDesc;
    reflector->GetDesc(&shaderDesc);

    // 入力レイアウト記述子の配列を作成（入力パラメーター数に応じて確保）
    D3D11_INPUT_ELEMENT_DESC* layoutDescs = new D3D11_INPUT_ELEMENT_DESC[shaderDesc.InputParameters];

    // 各入力パラメーターを解析
    for (UINT i = 0; i < shaderDesc.InputParameters; ++i)
    {
        D3D11_SIGNATURE_PARAMETER_DESC paramDesc;
        reflector->GetInputParameterDesc(i, &paramDesc);

        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

        // Auto match input type
        // 入力データの型に応じてDXGIフォーマットを決定
        switch (paramDesc.ComponentType)
        {
        case D3D_REGISTER_COMPONENT_FLOAT32:
            if (paramDesc.Mask == 1)
                format = DXGI_FORMAT_R32_FLOAT;
            else if (paramDesc.Mask <= 3)
                format = DXGI_FORMAT_R32G32_FLOAT;
            else if (paramDesc.Mask <= 7)
                format = DXGI_FORMAT_R32G32B32_FLOAT;
            else if (paramDesc.Mask <= 15)
                format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            break;

        case D3D_REGISTER_COMPONENT_SINT32:
            if (paramDesc.Mask == 1)
                format = DXGI_FORMAT_R32_SINT;
            else if (paramDesc.Mask <= 3)
                format = DXGI_FORMAT_R32G32_SINT;
            else if (paramDesc.Mask <= 7)
                format = DXGI_FORMAT_R32G32B32_SINT;
            else if (paramDesc.Mask <= 15)
                format = DXGI_FORMAT_R32G32B32A32_SINT;
            break;

        case D3D_REGISTER_COMPONENT_UINT32:
            if (paramDesc.Mask == 1)
                format = DXGI_FORMAT_R32_UINT;
            else if (paramDesc.Mask <= 3)
                format = DXGI_FORMAT_R32G32_UINT;
            else if (paramDesc.Mask <= 7)
                format = DXGI_FORMAT_R32G32B32_UINT;
            else if (paramDesc.Mask <= 15)
                format = DXGI_FORMAT_R32G32B32A32_UINT;
            break;
        }

        // 不明なフォーマットが指定された場合は処理を中止
        if (format == DXGI_FORMAT_UNKNOWN)
        {
            delete[] layoutDescs;
            return false;
        }

        // 入力要素の設定
        D3D11_INPUT_ELEMENT_DESC inputDesc = {};
        inputDesc.SemanticName = paramDesc.SemanticName;            // セマンティック名
        inputDesc.SemanticIndex = paramDesc.SemanticIndex;          // セマンティックインデックス
        inputDesc.Format = format;                                  // 型に応じたDXGIフォーマット
        inputDesc.InputSlot = 0;                                    // 入力スロット（通常0）
        inputDesc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT; // 自動でオフセット計算
        inputDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;     // 頂点ごとのデータ
        inputDesc.InstanceDataStepRate = 0;                         // インスタンシング未使用

        // 入力レイアウト記述子に追加
        layoutDescs[i] = inputDesc;
    }

    // 入力レイアウトを生成
    hr = device->CreateInputLayout(layoutDescs, shaderDesc.InputParameters,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), outLayout);

    // 一時メモリを解放し、リフレクションを終了
    delete[] layoutDescs;
    reflector->Release();

    return SUCCEEDED(hr);
}

bool ShaderLoader::CompileAndCreateVertexShader(
    ID3D11Device* device, 
    const char* path, 
    const char* entry, 
    ID3D11VertexShader** outVS, 
    ID3DBlob** outVSBlob)
{
    if (!CompileShaderFromFile(path, entry, SHADER_MODEL_VS, outVSBlob))
        return false;

    ID3D11VertexShader* newVS = nullptr;
    HRESULT hr = device->CreateVertexShader(
        (*outVSBlob)->GetBufferPointer(), 
        (*outVSBlob)->GetBufferSize(), 
        nullptr, 
        &newVS);

    if (SUCCEEDED(hr))
    {
        SafeRelease(outVS); // 古い VS を解放
        *outVS = newVS;
        return true;
    }

    SafeRelease(&newVS); // 作ったけど使えなかった場合も解放
    return false;
}

bool ShaderLoader::CompileAndCreatePixelShader(
    ID3D11Device* device,
    const char* path,
    const char* entry,
    ID3D11PixelShader** outPS)
{
    ID3DBlob* psBlob = nullptr;
    if (!CompileShaderFromFile(path, entry, SHADER_MODEL_PS, &psBlob))
        return false;

    ID3D11PixelShader* newPS = nullptr;
    HRESULT hr = device->CreatePixelShader(
        psBlob->GetBufferPointer(), 
        psBlob->GetBufferSize(), 
        nullptr, 
        &newPS);

    psBlob->Release();

    if (SUCCEEDED(hr))
    {
        SafeRelease(outPS); // 古い PS を解放
        *outPS = newPS;
        return true;
    }

    SafeRelease(&newPS);
    return false;
}

bool ShaderLoader::CompileAndCreateGeometryShader(
    ID3D11Device* device, 
    const char* path, 
    const char* entry, 
    ID3D11GeometryShader** outGS)
{
    ID3DBlob* gsBlob = nullptr;
    // HLSL ファイルをコンパイル（GS モデルで）
    if (!CompileShaderFromFile(path, entry, SHADER_MODEL_GS, &gsBlob))
        return false;

    ID3D11GeometryShader* newGS = nullptr;
    HRESULT hr = device->CreateGeometryShader(
        gsBlob->GetBufferPointer(),
        gsBlob->GetBufferSize(),
        nullptr,
        &newGS);

    gsBlob->Release();

    if (SUCCEEDED(hr))
    {
        SafeRelease(outGS); // 古い GS を解放
        *outGS = newGS;
        return true;
    }

    SafeRelease(&newGS);
    return false;
}

bool ShaderLoader::CompileAndCreateComputeShader(
    ID3D11Device* device, 
    const char* path, 
    const char* entry, 
    ID3D11ComputeShader** outCS)
{
    ID3DBlob* blob = nullptr;
    // .hlsl から CS をコンパイル
    if (!CompileShaderFromFile(path, entry, SHADER_MODEL_CS, &blob))
        return false;

    // コンピュートシェーダーの作成
    ID3D11ComputeShader* newCS = nullptr;
    HRESULT hr = device->CreateComputeShader(
        blob->GetBufferPointer(), 
        blob->GetBufferSize(), 
        nullptr, 
        &newCS);

    blob->Release();

    if (SUCCEEDED(hr))
    {
        SafeRelease(outCS);
        *outCS = newCS;
        return true;
    }

    SafeRelease(&newCS);
    return false;
}

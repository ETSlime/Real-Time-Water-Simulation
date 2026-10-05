#pragma once
//=============================================================================
//
// [ShaderInclude.h]
// Author : 
// 
//=============================================================================
#include "main.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define INCLUDE_ROOT    "Shaders/"

class ShaderInclude : public ID3DInclude
{
public:
    STDMETHOD(Open)(D3D_INCLUDE_TYPE includeType, LPCSTR fileName, LPCVOID parentData, LPCVOID* ppData, UINT* pBytes) override
    {
		// ファイル名が絶対パスでない場合、INCLUDE_ROOTを付加
        char fullPath[256] = {};
        strcpy_s(fullPath, INCLUDE_ROOT);
        strcat_s(fullPath, fileName);

		// ファイルをバイナリで開く
        FILE* file = nullptr;
        fopen_s(&file, fullPath, "rb");
        if (!file) return E_FAIL;

        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char* data = new char[size];
        fread(data, 1, size, file);
        fclose(file);

        *ppData = data;
        *pBytes = size;
        return S_OK;
    }

    STDMETHOD(Close)(LPCVOID pData) override
    {
        delete[](char*)pData;
        return S_OK;
    }
};

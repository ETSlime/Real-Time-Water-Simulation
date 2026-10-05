//=============================================================================
//
// [SaveSystem.cpp]
// Author : 
// 
//=============================================================================
#include "Core/SaveSystem.h"
#include "Core/GameSystem.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define SAVE_DIR  				"Save"
#define SAVE_FILE               "save.hex"

bool SaveSystem::SaveGame(void)
{
    SaveData state{};
    GetSaveData(state);

    char filePath[MAX_PATH];
    snprintf(filePath, sizeof(filePath), "%s\\%s", SAVE_DIR, SAVE_FILE);

	// ディレクトリが存在しない場合は作成
    if (!CreateDirectoryA(SAVE_DIR, NULL))
    {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) 
        {
			return false; // ディレクトリの作成に失敗
        }
    }

	// ファイルを開く
	FILE* fp = nullptr;
    if (fopen_s(&fp, filePath, "wb") != 0 || fp == nullptr)
        return false;

	WriteHex(fp, state.seedCount);
    WriteHex(fp, state.bulletCount);
    WriteHex(fp, state.keyCount);
    WriteHex(fp, state.hasBullet);
    WriteHex(fp, state.pos.x);
    WriteHex(fp, state.pos.y);
    WriteHex(fp, state.pos.z);

	WriteHex(fp, static_cast<UINT>(state.currentSceneID));

	fclose(fp);
	return true;
}

bool SaveSystem::LoadGame(SaveData& saveData)
{
    char filePath[MAX_PATH];
    snprintf(filePath, sizeof(filePath), "%s\\%s", SAVE_DIR, SAVE_FILE);

    // ファイルを開く
    FILE* fp = nullptr;
    if (fopen_s(&fp, filePath, "rb") != 0 || fp == nullptr)
        return false;

    bool ok = true;
    ok = ok && ReadHex(fp, saveData.seedCount);
    ok = ok && ReadHex(fp, saveData.bulletCount);
    ok = ok && ReadHex(fp, saveData.keyCount);
    ok = ok && ReadHex(fp, saveData.hasBullet);
    ok = ok && ReadHex(fp, saveData.pos.x);
    ok = ok && ReadHex(fp, saveData.pos.y);
    ok = ok && ReadHex(fp, saveData.pos.z);

    UINT scene = 0;
    ok = ok && ReadHex(fp, scene);

    fclose(fp);
    if (!ok) return false;
	saveData.currentSceneID = static_cast<SceneID>(scene);

    return true;
}

bool SaveSystem::CheckSaveDataExists(void)
{
    char filePath[MAX_PATH];
    snprintf(filePath, sizeof(filePath), "%s\\%s", SAVE_DIR, SAVE_FILE);

    FILE* fp = nullptr;
    if (fopen_s(&fp, filePath, "rb") != 0 || fp == nullptr)
    {
		return false; // ファイルが存在しない
    }

    fclose(fp);
	return true; // ファイルが存在する
}

void SaveSystem::GetSaveData(SaveData& saveData)
{
    Player* player = GameSystem::get_instance().GetPlayer();

	saveData.seedCount = player->GetPlayerAttributes().seedCount;
    saveData.bulletCount = player->GetPlayerAttributes().bulletCount;
    saveData.keyCount = player->GetPlayerAttributes().keyCount;
    saveData.hasBullet = player->GetPlayerAttributes().hasBullet;
	saveData.pos = player->GetTransform().pos;
	saveData.currentSceneID = Ground::get_instance().GetCurrentSceneID();


}

bool SaveSystem::ReadNextHexByte(FILE* fp, unsigned char& outByte)
{
    int c;

	// 16進数の文字が見つかるまで読み飛ばす
    do {
        c = fgetc(fp);
        if (c == EOF) return false;
    } while (!isxdigit(c));

    char buf[3];
    buf[0] = static_cast<char>(c);

	// 次の16進数の文字が見つかるまで読み飛ばす
    do {
        c = fgetc(fp);
        if (c == EOF) return false;
    } while (!isxdigit(c));
    buf[1] = static_cast<char>(c);
    buf[2] = '\0';

    unsigned int v = 0;
    
	// 16進数として解釈してバイト値に変換
    if (sscanf(buf, "%2X", &v) != 1) return false;

    outByte = static_cast<unsigned char>(v);
    return true;
}



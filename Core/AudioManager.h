#pragma once
//=============================================================================
//
// [AudioManager.h]
// Author : 
// 
//=============================================================================
#include "main.h"
#include "xaudio2.h"						// サウンド処理で必要
#include "Utility/SingletonBase.h"
#include "Utility/SimpleArray.h"

//*****************************************************************************
// 構造体定義
//*****************************************************************************
typedef struct
{
	char* pFilename;	// ファイル名
	int nCntLoop;		// ループカウント
} SOUNDPARAM;


//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define SOUND_LABEL_MAX 	(99)		// 音素材の数

enum class SoundLabel : int
{
	SOUND_LABEL_BGM_title,
	SOUND_LABEL_BGM_spring,
	SOUND_LABEL_BGM_summer,
	SOUND_LABEL_BGM_autumn,
	SOUND_LABEL_BGM_winter,
	SOUND_LABEL_BGM_boss,


	SOUND_LABEL_SE_player_melee_attack,
	SOUND_LABEL_SE_bullet,
	SOUND_LABEL_SE_sun_bullet,
	SOUND_LABEL_SE_boom_long,
	SOUND_LABEL_SE_boom_short,
	SOUND_LABEL_SE_switch_bullet,
	SOUND_LABEL_SE_switch_weapon,
	SOUND_LABEL_SE_get_seed,
	SOUND_LABEL_SE_get_key,
	SOUND_LABEL_SE_sun_bullet_stuck,
	SOUND_LABEL_SE_enemy_melee_attack,
	SOUND_LABEL_SE_enemy_projectile_attack,
	SOUND_LABEL_SE_enemy_hit_melee,
	SOUND_LABEL_SE_ui_hover,
	SOUND_LABEL_SE_ui_select,
	SOUND_LABEL_SE_throw,
	SOUND_LABEL_SE_boss_scream,
	SOUND_LABEL_SE_boss_cast,
	SOUND_LABEL_SE_boss_laser,
};

class AudioManager : public SingletonBase<AudioManager>
{
public:
	bool Init(HWND hWnd);
	void Shutdown(void);

	// セグメント再生(再生中なら停止)
	void PlaySound(SoundLabel label);

	// セグメント停止(ラベル指定)
	void StopSound(SoundLabel label);

	// セグメント停止(全て)
	void StopSound(void);

private:
	// チャンクのチェック
	HRESULT CheckChunk(HANDLE hFile, DWORD format, DWORD* pChunkSize, DWORD* pChunkDataPosition);
	// チャンクデータの読み込み
	HRESULT ReadChunkData(HANDLE hFile, void* pBuffer, DWORD dwBuffersize, DWORD dwBufferoffset);

	IXAudio2* m_pXAudio2 = NULL;								// XAudio2オブジェクトへのインターフェイス
	IXAudio2MasteringVoice* m_pMasteringVoice = NULL;			// マスターボイス
	IXAudio2SourceVoice* m_apSourceVoice[SOUND_LABEL_MAX] = {}; 			// ソースボイス
	BYTE* m_apDataAudio[SOUND_LABEL_MAX] = {}; // オーディオデータ
	DWORD m_aSizeAudio[SOUND_LABEL_MAX] = {}; // オーディオデータサイズ

	// 各音素材のパラメータ
	SimpleArray<SOUNDPARAM> m_aParam;

};
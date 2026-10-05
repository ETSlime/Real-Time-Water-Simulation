#pragma once
//=============================================================================
//
// [ISpriteUI.h]
// Author : 
// 
//=============================================================================
#include "main.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************

//==================================================================
// スプライトの全インターフェースをデリゲートするマクロ
//==================================================================
#define DELEGATE_SPRITE_ALL_INTERFACE(spritePtr)            \
    DELEGATE_SPRITE_TRANSFORM_INTERFACE(spritePtr)          \
    DELEGATE_SPRITE_COLOR_INTERFACE(spritePtr)              \
    DELEGATE_SPRITE_UV_INTERFACE(spritePtr)                 \
    DELEGATE_SPRITE_TEXTURE_INTERFACE(spritePtr)            \
    DELEGATE_SPRITE_VISIBILITY_INTERFACE(spritePtr)         \
    DELEGATE_SPRITE_SIZE_INTERFACE(spritePtr)

//==================================================================
// スプライトの各インターフェースをデリゲートするマクロ
//==================================================================
#define DELEGATE_SPRITE_TRANSFORM_INTERFACE(spritePtr)                                  \
    void SetPosition(const XMFLOAT2& pos) override { spritePtr->SetPosition(pos); }     \
    void SetPosition(float x, float y) override { spritePtr->SetPosition({x, y}); }     \
    const XMFLOAT2& GetPosition() const override { return spritePtr->GetPosition(); }   \
                                                                                        \
    void SetScale(const XMFLOAT2& scale) override { spritePtr->SetScale(scale); }       \
    void SetScale(float x, float y) override { spritePtr->SetScale({x, y}); }           \
    const XMFLOAT2& GetScale() const override { return spritePtr->GetScale(); }         \
                                                                                        \
    void SetRotation(float rot) override { spritePtr->SetRotation(rot); }               \
    float GetRotation(void) const override { return spritePtr->GetRotation(); }

#define DELEGATE_SPRITE_COLOR_INTERFACE(spritePtr)                                                      \
    void SetColor(const XMFLOAT4& color) override { spritePtr->SetColor(color); }                       \
    void SetColor(float r, float g, float b, float a) override { spritePtr->SetColor({ r, g, b, a }); } \
    const XMFLOAT4& GetColor(void) const override { return spritePtr->GetColor(); }

#define DELEGATE_SPRITE_UV_INTERFACE(spritePtr)                                                     \
    void SetUV(const XMFLOAT4& uv) override { spritePtr->SetUV(uv); }                               \
    void SetUV(float u, float v, float w, float h) override { spritePtr->SetUV({ u, v, w, h }); }   \
    const XMFLOAT4& GetUV(void) const override { return spritePtr->GetUV(); }

#define DELEGATE_SPRITE_TEXTURE_INTERFACE(spritePtr)                                        \
    void SetTexture(ID3D11ShaderResourceView* tex) override { spritePtr->SetTexture(tex); } \
    ID3D11ShaderResourceView* GetTexture(void) const override { return spritePtr->GetTexture(); }

#define DELEGATE_SPRITE_VISIBILITY_INTERFACE(spritePtr)                             \
    void SetVisible(bool visible) override { spritePtr->SetVisible(visible); }      \
    bool IsVisible(void) const override { return spritePtr->IsVisible(); }

#define DELEGATE_SPRITE_SIZE_INTERFACE(spritePtr)                               \
    void SetSize(const XMFLOAT2& size) override { spritePtr->SetSize(size); }   \
    void SetSize(float w, float h) override { spritePtr->SetSize({w, h}); }     \
    const XMFLOAT2& GetSize(void) const override { return spritePtr->GetSize(); }

//==================================================================
// スプライトの変形可能インターフェース
//==================================================================
class ISpriteTransformable
{
public:
    virtual ~ISpriteTransformable() {}

    // 座標
    virtual void SetPosition(const XMFLOAT2& pos) = 0;
	virtual void SetPosition(float x, float y) = 0;
    virtual const XMFLOAT2& GetPosition(void) const = 0;

    // スケールを設定
    virtual void SetScale(const XMFLOAT2& scale) = 0;
	virtual void SetScale(float x, float y) = 0;
    virtual const XMFLOAT2& GetScale(void) const = 0;

    // 回転
    virtual void SetRotation(float rot) = 0;
    virtual float GetRotation(void) const = 0;

	// サイズ
	virtual void SetSize(const XMFLOAT2& size) = 0;
	virtual void SetSize(float w, float h) = 0;
	virtual const XMFLOAT2& GetSize(void) const = 0;
};

//==================================================================
// スプライトの色変更可能インターフェース
//==================================================================
class ISpriteColorable
{
public:
    virtual ~ISpriteColorable() {}

    virtual void SetColor(const XMFLOAT4& color) = 0;
	virtual void SetColor(float r, float g, float b, float a) = 0;
    virtual const XMFLOAT4& GetColor(void) const = 0;
};

//==================================================================
// スプライトのテクスチャ可能インターフェース
//==================================================================
class ISpriteTexturable
{
public:
    virtual ~ISpriteTexturable() {}

    virtual void SetUV(const XMFLOAT4& uv) = 0;
	virtual void SetUV(float u, float v, float w, float h) = 0;
    virtual const XMFLOAT4& GetUV(void) const = 0;

    virtual void SetTexture(ID3D11ShaderResourceView* tex) = 0;
    virtual ID3D11ShaderResourceView* GetTexture(void) const = 0;
};

//==================================================================
// スプライトの可視性インターフェース
//==================================================================
class ISpriteVisible
{
public:
    virtual ~ISpriteVisible() {}

    virtual void SetVisible(bool visible) = 0;
    virtual bool IsVisible(void) const = 0;
};

//==================================================================
// スプライトのUIインターフェース
//==================================================================
class ISpriteUI : public ISpriteTransformable,
    public ISpriteColorable,
    public ISpriteTexturable,
    public ISpriteVisible {};
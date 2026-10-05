//=============================================================================
//
// EffectSystem処理 [EffectSystem.cpp]
// Author : 
//
//=============================================================================
#include "Effects/EffectSystem.h"
#include "Effects/SpecialEffects/SmokeEffectRenderer.h"
#include "Effects/SpecialEffects/FireBallEffectRenderer.h"
#include "Effects/SpecialEffects/Water/WaterFluidParticleRenderer.h"
#include "Effects/SpecialEffects/FallingParticleRenderer.h"

void EffectSystem::Init(void)
{
	m_device = Renderer::get_instance().GetDevice();
	m_context = Renderer::get_instance().GetDeviceContext();

	// あらかじめリザーブしてメモリ確保（推定数値）
	m_allEffects.reserve(MAX_EFFECT_NUM);
	m_billboardSimpleEffects.reserve(MAX_BILLBOARD_SIMPLE_NUM);
	m_billboardFlipbookEffects.reserve(MAX_BILLBOARD_FLIPBOOK_NUM);
	m_waterFluidEffects.reserve(MAX_WATER_FLUID_NUM);
	m_fallingParticleEffects.reserve(MAX_FALLING_PARTICLE_NUM);
	m_softBodyEffects.reserve(MAX_SOFTBODY_EFFECT_NUM);
	m_otherEffects.reserve(MAX_OTHER_EFFECT_NUM);

	m_initialized = true; // 初期化フラグを立てる
}

void EffectSystem::Shutdown(void)
{
	for (auto& effect : m_allEffects)
	{
		if (effect)
		{
			effect->Shutdown();
			SAFE_DELETE(effect);
		}
	}
}

void EffectSystem::Update(void)
{
	if (!m_initialized) return; // 初期化されていない場合は何もしない

	// 中身だけお掃除して再利用する
	m_toRemove.clear();

	// すべてのエフェクトちゃんをループして、更新または削除フラグをチェックする
	for (auto& effect : m_allEffects)
	{
		// シャットダウン済みのは後で消すために記録しておく
		if (effect->IsShutdown())
			m_toRemove.push_back(effect);
		else
			// 生きてるのは毎フレームちゃんとアップデート
			effect->Update();
	}

	// 安全のためにループが終わったあとで削除する
	for (auto* effect : m_toRemove)
	{
		RemoveEffect(effect);
	}
}

void EffectSystem::Draw(void)
{
	if (!m_initialized) return; // 初期化されていない場合は何もしない

	// 毎フレーム頭で分類クリア
	m_billboardSimpleEffects.clear();
	m_billboardFlipbookEffects.clear();
	m_softBodyEffects.clear();
	m_waterFluidEffects.clear();
	m_fallingParticleEffects.clear();
	m_otherEffects.clear();

	// 分類処理
	for (auto effect : m_allEffects)
	{
		EffectType type = effect->GetEffectType();
		if (IsParticleEffectType(type))
		{
			auto* particle = dynamic_cast<ParticleEffectRendererBase*>(effect);
			if (particle)
			{
				switch (particle->GetShaderGroupForEffect(type))
				{
				case ParticleShaderGroup::BillboardSimple:
					m_billboardSimpleEffects.push_back(particle);
					break;
				case ParticleShaderGroup::BillboardFlipbook:
					m_billboardFlipbookEffects.push_back(particle);
					break;
				case ParticleShaderGroup::WaterFluid:
					m_waterFluidEffects.push_back(particle);
					break;
				case ParticleShaderGroup::Falling:
					m_fallingParticleEffects.push_back(particle);
					break;
				default:
					break;
				}
			}
		}
		else
		{
			switch (effect->GetEffectType())
			{
			case EffectType::SoftBody:
				m_softBodyEffects.push_back(effect);
				break;
			default:
				m_otherEffects.push_back(effect);
				break;
			}
		}

	}

	// その他のエフェクト描画
	if (!m_otherEffects.empty())
	{
		// パイプライン状態をリセット
		m_context->GSSetShader(nullptr, nullptr, 0);

		for (auto other : m_otherEffects)
		{
			other->SetupPipeline();
			other->Draw();
		}
	}

	// BillboardSimple
	if (!m_billboardSimpleEffects.empty())
	{
		ParticleEffectRendererBase::ResetPipelineState(ParticleShaderGroup::BillboardSimple);
		for (auto* effect : m_billboardSimpleEffects)
		{
			if (effect->IsShutdown()) // シャットダウン済みならスキップ
				continue;
			effect->SetupPipeline();
			effect->Draw();
		}
	}

	// BillboardFlipbook
	if (!m_billboardFlipbookEffects.empty())
	{
		ParticleEffectRendererBase::ResetPipelineState(ParticleShaderGroup::BillboardFlipbook);
		for (auto* effect : m_billboardFlipbookEffects)
		{
			if (effect->IsShutdown()) // シャットダウン済みならスキップ
				continue;
			effect->SetupPipeline();
			effect->Draw();
		}
	}

	// WaterFluid
	if (!m_waterFluidEffects.empty())
	{
		ParticleEffectRendererBase::ResetPipelineState(ParticleShaderGroup::WaterFluid);
		for (auto* effect : m_waterFluidEffects)
		{
			effect->SetupPipeline();
			effect->Draw();
		}
	}

	// FallingParticle
	if (!m_fallingParticleEffects.empty())
	{
		ParticleEffectRendererBase::ResetPipelineState(ParticleShaderGroup::Falling);
		for (auto* effect : m_fallingParticleEffects)
		{
			effect->SetupPipeline();
			effect->Draw();
		}
	}


	EffectType lastType = EffectType::None; // 最初のダミー値

	// ソフトボディフェークト描画
	if (!m_softBodyEffects.empty())
	{
		if (lastType != EffectType::SoftBody)
		{
			m_context->GSSetShader(nullptr, nullptr, 0);
			//SoftBodyRenderer::ResetPipelineState(); // クラス単位でリセット
		}

		for (auto softBody : m_softBodyEffects)
		{
			softBody->SetupPipeline();
			softBody->Draw();
		}

		lastType = EffectType::SoftBody;
	}	

	// 最後にパイプライン状態をリセット
	ClearAllEffectBindings();
}

ParticleEffectRendererBase* EffectSystem::SpawnParticleEffect(ParticleEffectParams& params)
{
	if (m_allEffects.getSize() >= MAX_EFFECT_NUM)
		return nullptr; // 最大数に達している場合は生成しない

	// パーティクル数の制限
	if (params.numParticles > MAX_PARTICLES)
		params.numParticles = MAX_PARTICLES; // 最大数に制限
	else if (params.numParticles < 1)
		params.numParticles = 1; // 最小数に制限

	ParticleEffectRendererBase* effect = nullptr;

	// タイプに応じてインスタンス生成
	switch (params.type)
	{
	case EffectType::Fire:
		//effect = new FireEffectRenderer();
		break;
	case EffectType::Smoke:
		effect = new SmokeEffectRenderer();
		break;
	case EffectType::FireBall:
		effect = new FireBallEffectRenderer();
		break;
	case EffectType::WaterFluid:
		effect = new WaterFluidParticleRenderer();
		break;
	case EffectType::Falling:
		effect = new FallingParticleRenderer();
		break;
	default:
		return nullptr; // 未対応タイプ
	}

	if (!effect)
		return nullptr;

	effect->ConfigureEffect(params); // 初期化
	if (effect->Initialize(m_device, m_context))
	{
		m_allEffects.push_back(effect);
		return effect;
	}
	else
	{
		SAFE_DELETE(effect);
		return nullptr;
	}
}

void EffectSystem::AddEffect(IEffectRenderer* effect)
{
	if (!m_initialized)
	{
		Init(); // 初期化されていない場合は初期化
	}
	if (m_allEffects.getSize() >= MAX_EFFECT_NUM)
		return; // 最大数に達している場合は追加しない
	if (effect)
	{
		m_allEffects.push_back(effect);
	}
}

void EffectSystem::RemoveEffect(IEffectRenderer* effect)
{
	int index = m_allEffects.find_index(effect);
	if (index >= 0)
	{
		m_allEffects[index]->Shutdown();
		SAFE_DELETE(m_allEffects[index]);
		m_allEffects.erase(index);
	}
}

void EffectSystem::ClearAllEffectBindings(void)
{
	ID3D11ShaderResourceView* nullSRV = nullptr;

	m_ShaderResourceBinder.BindShaderResource(ShaderStage::VS, SLOT_SRV_PARTICLE, nullSRV);
	m_ShaderResourceBinder.BindShaderResource(ShaderStage::PS, SLOT_TEX_DIFFUSE, nullSRV);
	m_context->GSSetShader(nullptr, nullptr, 0);
}

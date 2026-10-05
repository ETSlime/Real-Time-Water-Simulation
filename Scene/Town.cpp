//=============================================================================
//
// Townèàóù [Town.cpp]
// Author : 
//
//=============================================================================
#include "Town.h"
#include "Core/Graphics/ShadowMeshCollector.hpp"

// ì«Ç›çûÇﬁÉÇÉfÉãñº
#define	MODEL_ENVIRONMENT_PATH		"data/MODEL/Environment/"
#define	MODEL_TOWN_PATH				"data/MODEL/Environment/Knight"
#define	MODEL_CHURCH_PATH			"data/MODEL/Environment/Church"
#define	MODEL_SKYBOX_NAME			"skybox.fbx"
#define	MODEL_LOD0_NAME				"LOD0.fbx"
#define	MODEL_LOD1_NAME				"LOD1.fbx"
#define	MODEL_LOD2_NAME				"LOD2.fbx"
#define	MODEL_CHURCH_NAME			"church.fbx"

#define SIZE_SCALE				(0.5f)
#define SKYBOX_SIZE				(225850.0f)
#define PLOT_LOD0_SIZE			(5250.0f * SIZE_SCALE)
#define PLOT_LOD1_SIZE			(5250.0f * SIZE_SCALE)
#define PLOT_LOD2_SIZE			(18850.0f * SIZE_SCALE)
#define CHURCH_SIZE			(5250.0f)

Town::Town()
{
	SkinnedGameObjectConfig config;

	ZeroMemory(&config, sizeof(SkinnedGameObjectConfig));
	GameObject<SkinnedMeshModelInstance>* skyBox = new GameObject<SkinnedMeshModelInstance>();
	config.modelPath = MODEL_TOWN_PATH;
	config.modelName = MODEL_SKYBOX_NAME;
	config.modelType = SkinnedModelType::Skybox;
	config.scale = XMFLOAT3(SKYBOX_SIZE, SKYBOX_SIZE, SKYBOX_SIZE);
	config.castShadow = false;
	skyBox->Instantiate(config);
	//skyBox->GetSkinnedMeshModel()->BuildTrianglesByWorldMatrix(worldMatrix);
	//skyBox->GetSkinnedMeshModel()->BuildOctree();

	models.push_back(skyBox);

	ZeroMemory(&config, sizeof(SkinnedGameObjectConfig));
	GameObject<SkinnedMeshModelInstance>* LoD0 = new GameObject<SkinnedMeshModelInstance>();
	config.modelPath = MODEL_TOWN_PATH;
	config.modelName = MODEL_LOD0_NAME;
	config.modelType = SkinnedModelType::Town_LOD0;
	config.scale = XMFLOAT3(PLOT_LOD0_SIZE, PLOT_LOD0_SIZE, PLOT_LOD0_SIZE);
	config.position = XMFLOAT3(PLOT_LOD0_SIZE * 0.63f, PLOT_LOD0_SIZE * 0.2f, -PLOT_LOD0_SIZE * 2.14f);
	config.rotation = XMFLOAT3(0, -XM_PI * 0.2f, 0.0f);
	config.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	LoD0->Instantiate(config);
	//LoD0->GetSkinnedMeshModel()->LoadTownTexture();
	models.push_back(LoD0);

	ZeroMemory(&config, sizeof(SkinnedGameObjectConfig));
	GameObject<SkinnedMeshModelInstance>* LoD1 = new GameObject<SkinnedMeshModelInstance>();
	config.modelPath = MODEL_TOWN_PATH;
	config.modelName = MODEL_LOD1_NAME;
	config.modelType = SkinnedModelType::Town_LOD1;
	config.scale = XMFLOAT3(PLOT_LOD1_SIZE, PLOT_LOD1_SIZE, PLOT_LOD1_SIZE);
	config.position = XMFLOAT3(PLOT_LOD1_SIZE * 0.76f, -PLOT_LOD1_SIZE * 0.26f, -PLOT_LOD1_SIZE * 1.34f);
	config.rotation = XMFLOAT3(0, -XM_PI * 0.5f, 0.0f);
	config.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	LoD1->Instantiate(config);
	//LoD1->GetSkinnedMeshModel()->LoadTownTexture();
	models.push_back(LoD1);

	ZeroMemory(&config, sizeof(SkinnedGameObjectConfig));
	GameObject<SkinnedMeshModelInstance>* LoD2 = new GameObject<SkinnedMeshModelInstance>();
	config.modelPath = MODEL_TOWN_PATH;
	config.modelName = MODEL_LOD2_NAME;
	config.modelType = SkinnedModelType::Town_LOD2;
	config.scale = XMFLOAT3(PLOT_LOD2_SIZE, PLOT_LOD2_SIZE, PLOT_LOD2_SIZE);
	config.position = XMFLOAT3(PLOT_LOD2_SIZE * 0.2f, -PLOT_LOD2_SIZE * 0.15f, -PLOT_LOD2_SIZE * 0.5f);
	config.rotation = XMFLOAT3(0, -XM_PI * 0.5f, 0.0f);
	config.collisionType = ObjectCollisionType::MESH_BOUNDING_BOX;
	LoD2->Instantiate(config);
	//LoD2->GetSkinnedMeshModel()->LoadTownTexture();
	models.push_back(LoD2);

	//GameObject<SkinnedMeshModelInstance>* church = new GameObject<SkinnedMeshModelInstance>();
	//church->Instantiate(MODEL_CHURCH_PATH, MODEL_CHURCH_NAME, ModelType::Church);
	//church->GetSkinnedMeshModel()->SetDrawBoundingBox(false);
	//church->SetScale(XMFLOAT3(CHURCH_SIZE, CHURCH_SIZE, CHURCH_SIZE));
	//church->GetSkinnedMeshModel()->LoadTownTexture();
	//church->Update();
	//worldMatrix = church->GetWorldMatrix();
	//church->GetSkinnedMeshModel()->BuildTrianglesByWorldMatrix(worldMatrix);
	//church->GetSkinnedMeshModel()->BuildOctree();
	//church->SetRenderShadow(false);

	//models.push_back(church);

	//GameObject<SkinnedMeshModelInstance>* plot5 = new GameObject<SkinnedMeshModelInstance>();
	//plot5->Instantiate(MODEL_TOWN_PATH, MODEL_PLOT2_NAME);
	//plot5->GetSkinnedMeshModel()->SetDrawBoundingBox(false);
	//plot5->SetScale(XMFLOAT3(PLOT_SIZE, PLOT_SIZE, PLOT_SIZE));
	//plot5->Update();
	//worldMatrix = plot5->GetWorldMatrix();
	//plot5->GetSkinnedMeshModel()->BuildTrianglesByWorldMatrix(worldMatrix);
	//plot5->GetSkinnedMeshModel()->BuildOctree();
	//plot5->SetRenderShadow(false);

	//models.push_back(plot5);

	//GameObject<SkinnedMeshModelInstance>* plot9 = new GameObject<SkinnedMeshModelInstance>();
	//plot9->Instantiate(MODEL_TOWN_PATH, MODEL_PLOT2_NAME);
	//plot9->GetSkinnedMeshModel()->SetDrawBoundingBox(false);
	//plot9->SetScale(XMFLOAT3(PLOT_SIZE, PLOT_SIZE, PLOT_SIZE));
	//plot9->Update();
	//worldMatrix = plot9->GetWorldMatrix();
	//plot9->GetSkinnedMeshModel()->BuildTrianglesByWorldMatrix(worldMatrix);
	//plot9->GetSkinnedMeshModel()->BuildOctree();
	//plot9->SetRenderShadow(false);

	//models.push_back(plot9);

}

void Town::Update()
{
	int modelCnt = models.getSize();
	for (int i = 0; i < modelCnt; i++)
		models[i]->Update();
}

void Town::Draw()
{
	int modelCnt = models.getSize();
	for (int i = 0; i < modelCnt; i++)
	{
		if (m_Renderer.GetRenderMode() == RenderMode::SKINNED_MESH)
		{
			if (models[i]->GetSkinnedMeshModel()->GetModelType() == SkinnedModelType::Skybox)
			{
				m_Camera.SetCameraType(CameraType::SKYBOX);
				models[i]->Draw();
				m_Camera.SetCameraType(CameraType::SCENE);
			}
			else
				models[i]->Draw();

		}

	}

}

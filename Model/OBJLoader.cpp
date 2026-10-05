//=============================================================================
//
// [OBJLoader.cpp]
// Author : 
// 
//=============================================================================
#include "Model/OBJLoader.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define	VALUE_MOVE_MODEL			(0.50f)					// 移動速度
#define	RATE_MOVE_MODEL				(0.20f)					// 移動慣性係数
#define	VALUE_ROTATE_MODEL			(XM_PI * 0.05f)			// 回転速度
#define	RATE_ROTATE_MODEL			(0.20f)					// 回転慣性係数
#define	SCALE_MODEL					(10.0f)					// 回転慣性係数

bool OBJLoader::LoadObjModel(const char* FileName, StaticModelData* Model, AxisFlip flip)
{

	XMFLOAT3* positionArray;
	XMFLOAT3* normalArray;
	XMFLOAT2* texcoordArray;

	unsigned int	positionNum = 0;
	unsigned int	normalNum = 0;
	unsigned int	texcoordNum = 0;
	unsigned int	vertexNum = 0;
	unsigned int	indexNum = 0;
	unsigned int	in = 0;
	unsigned int	subsetNum = 0;

	MODEL_MATERIAL* materialArray = NULL;
	unsigned int	materialNum = 0;

	char str[256];
	char* s;
	char c;


	FILE* file;
	file = fopen(FileName, "rt");
	if (file == NULL)
	{
		printf("エラー:LoadModel %s \n", FileName);
		return false;
	}



	//要素数カウント
	while (TRUE)
	{
		fscanf(file, "%s", str);

		if (feof(file) != 0)
			break;

		if (strcmp(str, "v") == 0)
		{
			positionNum++;
		}
		else if (strcmp(str, "vn") == 0)
		{
			normalNum++;
		}
		else if (strcmp(str, "vt") == 0)
		{
			texcoordNum++;
		}
		else if (strcmp(str, "usemtl") == 0)
		{
			subsetNum++;
		}
		else if (strcmp(str, "f") == 0)
		{
			in = 0;

			do
			{
				fscanf(file, "%s", str);
				vertexNum++;
				in++;
				c = fgetc(file);
			} while (c != '\n' && c != '\r');

			//四角は三角に分割
			if (in == 4)
				in = 6;

			indexNum += in;
		}
	}


	//メモリ確保
	positionArray = new XMFLOAT3[positionNum];
	normalArray = new XMFLOAT3[normalNum];
	texcoordArray = new XMFLOAT2[texcoordNum];


	Model->VertexArray = new VERTEX_3D[vertexNum];
	Model->VertexNum = vertexNum;

	Model->IndexArray = new unsigned int[indexNum];
	Model->IndexNum = indexNum;

	Model->SubsetArray = new SUBSET[subsetNum];
	Model->SubsetNum = subsetNum;


	Model->boundingBox.minPoint = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
	Model->boundingBox.maxPoint = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);


	//要素読込
	XMFLOAT3* position = positionArray;
	XMFLOAT3* normal = normalArray;
	XMFLOAT2* texcoord = texcoordArray;

	unsigned int vc = 0;
	unsigned int ic = 0;
	unsigned int sc = 0;


	fseek(file, 0, SEEK_SET);

	while (TRUE)
	{
		fscanf(file, "%s", str);

		if (feof(file) != 0)
			break;

		if (strcmp(str, "mtllib") == 0)
		{
			//マテリアルファイル
			fscanf(file, "%s", str);

			char path[256];
			//	strcpy( path, "data/model/" );

				//----------------------------------- フォルダー対応
			strcpy(path, FileName);
			char* adr = path;
			char* ans = adr;
			while (1)
			{
				adr = strstr(adr, "/");
				if (adr == NULL) break;
				else ans = adr;
				adr++;
			}
			if (path != ans) ans++;
			*ans = 0;
			//-----------------------------------

			strcat(path, str);

			LoadMaterial(path, &materialArray, &materialNum);
		}
		else if (strcmp(str, "o") == 0)
		{
			//オブジェクト名
			fscanf(file, "%s", str);
		}
		else if (strcmp(str, "v") == 0)
		{
			//頂点座標
			fscanf(file, "%f", &position->x);
			fscanf(file, "%f", &position->y);
			fscanf(file, "%f", &position->z);
			position->x *= SCALE_MODEL;
			position->y *= SCALE_MODEL;
			position->z *= SCALE_MODEL;

			if (HasFlip(flip, AxisFlip::FlipX))
				position->x *= -1.0f;
			if (HasFlip(flip, AxisFlip::FlipY))
				position->y *= -1.0f;
			if (HasFlip(flip, AxisFlip::FlipZ))
				position->z *= -1.0f;


			// Update bounding box
			Model->boundingBox.minPoint.x = min(Model->boundingBox.minPoint.x, position->x);
			Model->boundingBox.minPoint.y = min(Model->boundingBox.minPoint.y, position->y);
			Model->boundingBox.minPoint.z = min(Model->boundingBox.minPoint.z, position->z);

			Model->boundingBox.maxPoint.x = max(Model->boundingBox.maxPoint.x, position->x);
			Model->boundingBox.maxPoint.y = max(Model->boundingBox.maxPoint.y, position->y);
			Model->boundingBox.maxPoint.z = max(Model->boundingBox.maxPoint.z, position->z);


			position++;
		}
		else if (strcmp(str, "vn") == 0)
		{
			//法線
			fscanf(file, "%f", &normal->x);
			fscanf(file, "%f", &normal->y);
			fscanf(file, "%f", &normal->z);

			if (HasFlip(flip, AxisFlip::FlipX))
				normal->x *= -1.0f;
			if (HasFlip(flip, AxisFlip::FlipY))
				normal->y *= -1.0f;
			if (HasFlip(flip, AxisFlip::FlipZ))
				normal->z *= -1.0f;

			normal++;
		}
		else if (strcmp(str, "vt") == 0)
		{
			//テクスチャ座標
			fscanf(file, "%f", &texcoord->x);
			fscanf(file, "%f", &texcoord->y);
			texcoord->y = 1.0f - texcoord->y;
			texcoord++;
		}
		else if (strcmp(str, "usemtl") == 0)
		{
			//マテリアル
			fscanf(file, "%s", str);

			if (sc != 0)
				Model->SubsetArray[sc - 1].IndexNum = ic - Model->SubsetArray[sc - 1].StartIndex;

			Model->SubsetArray[sc].StartIndex = ic;


			for (unsigned int i = 0; i < materialNum; i++)
			{
				if (strcmp(str, materialArray[i].Name) == 0)
				{
					Model->SubsetArray[sc].Material.MaterialData = materialArray[i].MaterialData;
					Model->SubsetArray[sc].Material.MaterialData.LoadMaterial = TRUE;
					strcpy(Model->SubsetArray[sc].Material.DiffuseTextureName, materialArray[i].DiffuseTextureName);
					strcpy(Model->SubsetArray[sc].Material.NormalTextureName, materialArray[i].NormalTextureName);
					strcpy(Model->SubsetArray[sc].Material.BumpTextureName, materialArray[i].BumpTextureName);
					strcpy(Model->SubsetArray[sc].Material.OpacityTextureName, materialArray[i].OpacityTextureName);
					strcpy(Model->SubsetArray[sc].Material.ReflectTextureName, materialArray[i].ReflectTextureName);
					strcpy(Model->SubsetArray[sc].Material.TranslucencyTextureName, materialArray[i].TranslucencyTextureName);
					strcpy(Model->SubsetArray[sc].Material.Name, materialArray[i].Name);

					break;
				}
			}

			sc++;

		}
		else if (strcmp(str, "f") == 0)
		{
			//面
			in = 0;

			do
			{
				fscanf(file, "%s", str);

				s = strtok(str, "/");
				Model->VertexArray[vc].Position = positionArray[atoi(s) - 1];
				if (s[strlen(s) + 1] != '/')
				{
					//テクスチャ座標が存在しない場合もある
					s = strtok(NULL, "/");
					Model->VertexArray[vc].TexCoord = texcoordArray[atoi(s) - 1];
				}
				s = strtok(NULL, "/");
				Model->VertexArray[vc].Normal = normalArray[atoi(s) - 1];

				Model->VertexArray[vc].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

				Model->IndexArray[ic] = vc;
				ic++;
				vc++;

				in++;
				c = fgetc(file);
			} while (c != '\n' && c != '\r');

			//四角は三角に分割
			if (in == 4)
			{
				Model->IndexArray[ic] = vc - 4;
				ic++;
				Model->IndexArray[ic] = vc - 2;
				ic++;
			}
		}
	}

	if (m_loadTangent == false)
	{
		for (unsigned int i = 0; i < indexNum; i += 3)
		{
			VERTEX_3D v0 = Model->VertexArray[Model->IndexArray[i]];
			VERTEX_3D v1 = Model->VertexArray[Model->IndexArray[i + 1]];
			VERTEX_3D v2 = Model->VertexArray[Model->IndexArray[i + 2]];

			XMVECTOR edge1 = DirectX::XMLoadFloat3(&v1.Position) - DirectX::XMLoadFloat3(&v0.Position);
			XMVECTOR edge2 = DirectX::XMLoadFloat3(&v2.Position) - DirectX::XMLoadFloat3(&v0.Position);

			float deltaU1 = v1.TexCoord.x - v0.TexCoord.x;
			float deltaV1 = v1.TexCoord.y - v0.TexCoord.y;
			float deltaU2 = v2.TexCoord.x - v0.TexCoord.x;
			float deltaV2 = v2.TexCoord.y - v0.TexCoord.y;

			float f = 1.0f / (deltaU1 * deltaV2 - deltaU2 * deltaV1);
			XMVECTOR tangent = f * (deltaV2 * edge1 - deltaV1 * edge2);

			XMStoreFloat3(&v0.Tangent, tangent);
			XMStoreFloat3(&v1.Tangent, tangent);
			XMStoreFloat3(&v2.Tangent, tangent);

			Model->VertexArray[Model->IndexArray[i]] = v0;
			Model->VertexArray[Model->IndexArray[i + 1]] = v1;
			Model->VertexArray[Model->IndexArray[i + 2]] = v2;
		}
	}

	if (sc != 0)
		Model->SubsetArray[sc - 1].IndexNum = ic - Model->SubsetArray[sc - 1].StartIndex;




	SAFE_DELETE_ARRAY(positionArray);
	SAFE_DELETE_ARRAY(normalArray);
	SAFE_DELETE_ARRAY(texcoordArray);
	SAFE_DELETE_ARRAY(materialArray);

	fclose(file);

	return true;
}


void OBJLoader::LoadMaterial(char* FileName, MODEL_MATERIAL** MaterialArray, unsigned int* MaterialNum)
{
	char str[256];

	FILE* file;
	file = fopen(FileName, "rt");
	if (file == NULL)
	{
		printf("エラー:LoadMaterial %s \n", FileName);
		return;
	}

	MODEL_MATERIAL* materialArray;
	unsigned int materialNum = 0;

	//要素数カウント
	while (TRUE)
	{
		fscanf(file, "%s", str);

		if (feof(file) != 0)
			break;


		if (strcmp(str, "newmtl") == 0)
		{
			materialNum++;
		}
	}


	//メモリ確保
	materialArray = new MODEL_MATERIAL[materialNum];
	ZeroMemory(materialArray, sizeof(MODEL_MATERIAL) * materialNum);


	//要素読込
	int mc = -1;

	fseek(file, 0, SEEK_SET);

	while (TRUE)
	{
		fscanf(file, "%s", str);

		if (feof(file) != 0)
			break;


		if (strcmp(str, "newmtl") == 0)
		{
			//マテリアル名
			mc++;
			fscanf(file, "%s", materialArray[mc].Name);
			strcpy(materialArray[mc].DiffuseTextureName, "");
			strcpy(materialArray[mc].NormalTextureName, "");
			strcpy(materialArray[mc].OpacityTextureName, "");
			strcpy(materialArray[mc].ReflectTextureName, "");
			strcpy(materialArray[mc].TranslucencyTextureName, "");
			materialArray[mc].MaterialData.noTexSampling = 1;
		}
		else if (strcmp(str, "Ka") == 0)
		{
			//アンビエント
			fscanf(file, "%f", &materialArray[mc].MaterialData.Ambient.x);
			fscanf(file, "%f", &materialArray[mc].MaterialData.Ambient.y);
			fscanf(file, "%f", &materialArray[mc].MaterialData.Ambient.z);
			materialArray[mc].MaterialData.Ambient.w = 1.0f;
		}
		else if (strcmp(str, "Kd") == 0)
		{
			//ディフューズ
			fscanf(file, "%f", &materialArray[mc].MaterialData.Diffuse.x);
			fscanf(file, "%f", &materialArray[mc].MaterialData.Diffuse.y);
			fscanf(file, "%f", &materialArray[mc].MaterialData.Diffuse.z);

			// Mayaでテクスチャを貼ると0.0fになっちゃうみたいなので
			if ((materialArray[mc].MaterialData.Diffuse.x + materialArray[mc].MaterialData.Diffuse.y + materialArray[mc].MaterialData.Diffuse.z) == 0.0f)
			{
				materialArray[mc].MaterialData.Diffuse.x = materialArray[mc].MaterialData.Diffuse.y = materialArray[mc].MaterialData.Diffuse.z = 1.0f;
			}

			materialArray[mc].MaterialData.Diffuse.w = 1.0f;
		}
		else if (strcmp(str, "Ks") == 0)
		{
			//スペキュラ
			fscanf(file, "%f", &materialArray[mc].MaterialData.Specular.x);
			fscanf(file, "%f", &materialArray[mc].MaterialData.Specular.y);
			fscanf(file, "%f", &materialArray[mc].MaterialData.Specular.z);
			materialArray[mc].MaterialData.Specular.w = 1.0f;
		}
		else if (strcmp(str, "Ns") == 0)
		{
			//スペキュラ強度
			fscanf(file, "%f", &materialArray[mc].MaterialData.Shininess);
		}
		else if (strcmp(str, "d") == 0)
		{
			//アルファ
			fscanf(file, "%f", &materialArray[mc].MaterialData.Diffuse.w);
		}
		else if (strcmp(str, "map_Kd") == 0)
		{
			LoadTextureName(FileName, file, materialArray, mc, TextureType::Diffuse);
		}
		else if (strcmp(str, "norm") == 0)
		{
			LoadTextureName(FileName, file, materialArray, mc, TextureType::Normal);
		}
		else if (strcmp(str, "map_d") == 0)
		{
			LoadTextureName(FileName, file, materialArray, mc, TextureType::Opacity);
		}
		else if (strcmp(str, "map_refl") == 0 || strcmp(str, "map_Reflect") == 0)
		{
			LoadTextureName(FileName, file, materialArray, mc, TextureType::Reflect);
		}
		else if (strcmp(str, "map_Bump") == 0)
		{
			LoadTextureName(FileName, file, materialArray, mc, TextureType::Bump);
		}
		else if (strcmp(str, "map_Translucency") == 0)
		{
			LoadTextureName(FileName, file, materialArray, mc, TextureType::Translucency);
		}
	}


	*MaterialArray = materialArray;
	*MaterialNum = materialNum;

	fclose(file);
}

void OBJLoader::LoadTextureName(char* FileName, FILE* file, MODEL_MATERIAL* Material, int mc, TextureType type)
{
	char str[256];

	//テクスチャ
	fscanf(file, "%s", str);

	char path[256];
	//	strcpy( path, "data/model/" );

		//----------------------------------- フォルダー対応
	strcpy(path, FileName);
	char* adr = path;
	char* ans = adr;
	while (1)
	{
		adr = strstr(adr, "/");
		if (adr == NULL) break;
		else ans = adr;
		adr++;
	}
	if (path != ans) ans++;
	*ans = 0;
	//-----------------------------------

	strcat(path, str);

	switch (type)
	{
	case TextureType::Diffuse:
		strcpy(Material[mc].DiffuseTextureName, path);
		Material[mc].MaterialData.noTexSampling = 0;
		break;
	case TextureType::Normal:
		strcpy(Material[mc].NormalTextureName, path);
		Material[mc].MaterialData.normalMapSampling = 1;
		break;
	case TextureType::Bump:
		strcpy(Material[mc].BumpTextureName, path);
		Material[mc].MaterialData.bumpMapSampling = 1;
		break;
	case TextureType::Opacity:
		strcpy(Material[mc].OpacityTextureName, path);
		Material[mc].MaterialData.opacityMapSampling = 1;
		break;
	case TextureType::Reflect:
		strcpy(Material[mc].ReflectTextureName, path);
		Material[mc].MaterialData.reflectMapSampling = 1;
		break;
	case TextureType::Translucency:
		strcpy(Material[mc].TranslucencyTextureName, path);
		Material[mc].MaterialData.translucencyMapSampling = 1;
		break;
	}
}
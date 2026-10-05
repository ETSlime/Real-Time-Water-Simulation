//=============================================================================
//
// FbxLoader処理 [FbxLoader.cpp]
// Author : 
//
//=============================================================================
#include "Model/FBXLoader.h"
#include "Model/SkinnedMeshModel.h"
//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define LEFT_HAND_BONE_IDX  (16)
#define RIGHT_HAND_BONE_IDX (20)

bool FBXLoader::LoadModel(SkinnedMeshModelData* modelData, const char* modelPath, const char* modelName, const char* texturePath, AnimClipName animName, SkinnedModelType modelType)
{
    char* modelFullPath = new char[MODEL_NAME_LENGTH]{};

    strcat(modelFullPath, modelPath);

    size_t modelPathLength = strlen(modelPath);
    if (modelPathLength > 0 && modelPath[modelPathLength - 1] != '/' && modelPath[modelPathLength - 1] != '\\') 
    {
        strcat(modelFullPath, "/");
    }
   
    strcat(modelFullPath, modelName);

	FILE* file;
	file = fopen(modelFullPath, "rt");
	if (file == NULL)
	{
		printf("エラー:LoadModel %s \n", modelFullPath);
		return false;
	}

    modelData->modelPath = const_cast<char*>(modelPath);
    modelData->modelName = const_cast<char*>(modelName);

    FbxNode* rootNode = new FbxNode(modelData->currentRootNodeID);
    modelData->fbxNodes.insert(modelData->currentRootNodeID, rootNode);
    modelData->currentRootNodeID++;

    modelData->modelType = modelType;

    if (animName != AnimClipName::ANIM_NONE)
    {
        modelData->currentAnimClip = new AnimationClip();
        modelData->currentAnimClip->name = animName;
    }

    modelData->leftHandTransformIdx = LEFT_HAND_BONE_IDX;
    modelData->rightHandTransformIdx = RIGHT_HAND_BONE_IDX;


    char buffer[1024];
    bool loadSuccess = true;
    long int position = 0;
    while (fgets(buffer, sizeof(buffer), file)) 
    {

        if (strstr(buffer, "; Object definitions"))
        {
            loadSuccess = ParseObjectDefinitions(file, modelData);
        }
        else if (strstr(buffer, "; Object properties"))
        {
            loadSuccess = ParseObjectProperties(file, modelData);
        }
        else if (strstr(buffer, "; Object connections"))
        {
            loadSuccess = ParseObjectConnections(file, modelData);
        }

        if (!loadSuccess)
        {
            return false;
        }

        position = ftell(file);
        memset(buffer, 0, sizeof(buffer));
    }



    SimpleArray<FbxNode*> rootNodeChilds;
    for (auto& node : modelData->fbxNodes)
    {
        if (node.key == modelData->currentRootNodeID - 1)
        {
            rootNodeChilds = node.value->childNodes;
        }
        else if (node.value->nodeType == FbxNodeType::LimbNode)
        {
            FbxNode* geometryNode = GetGeometryNodeByLimbNode(node.value);
            if (geometryNode)
            {
                MeshData** ppMeshData = modelData->meshDataMap.search(geometryNode->nodeID);
                (*ppMeshData)->limbCnt++;
            }

        }
    }

    //for (auto it : model.meshDataMap)
    //{
    //    ModelData* modelData = it.value;

    //    modelData->mModelHierarchy.resize(modelData->limbCnt);
    //    modelData->mModelGlobalRot.resize(modelData->limbCnt);
    //    modelData->mModelGlobalScl.resize(modelData->limbCnt);
    //    modelData->mModelTranslate.resize(modelData->limbCnt);
    //    modelData->mModelGlobalTrans.resize(modelData->limbCnt);
    //    modelData->mModelLocalTrans.resize(modelData->limbCnt);
    //}


    if (rootNodeChilds.getSize() != 0)
    {
        for (UINT i = 0; i < rootNodeChilds.getSize(); i++)
        {
            if (rootNodeChilds[i]->nodeType == FbxNodeType::Mesh)
            {
                HandleMeshNode(rootNodeChilds[i], modelData);
            }
            else if (rootNodeChilds[i]->nodeType == FbxNodeType::None)
            {
                SimpleArray<FbxNode*> childNodeChilds = rootNodeChilds[i]->childNodes;
                for (UINT j = 0; j < childNodeChilds.getSize(); j++)
                {
                    if (childNodeChilds[j]->nodeType == FbxNodeType::Mesh)
                    {
                        HandleMeshNode(childNodeChilds[j], modelData);
                    }
                    else if (childNodeChilds[j]->nodeType == FbxNodeType::None)
                    {
                        SimpleArray<FbxNode*> childchildNodeChilds = childNodeChilds[j]->childNodes;
                        for (UINT k = 0; k < childchildNodeChilds.getSize(); k++)
                        {
                            if (childchildNodeChilds[k]->nodeType == FbxNodeType::Mesh)
                            {
                                HandleMeshNode(childchildNodeChilds[k], modelData);
                            }
                        }
                    }
                }
            }
        }
    }

    for (auto& it : modelData->meshDataMap)
    {
        modelData->MeshDataCnt++;
        MeshData* meshData = it.value;
        
        XMFLOAT3 minPoint = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
        XMFLOAT3 maxPoint = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);


        char* diffuseTextureName = nullptr;
        if (meshData->material)
            diffuseTextureName = GetTextureName(modelData->modelPath, meshData->material->DiffuseColorTexName);

        meshData->diffuseTextureName = diffuseTextureName;
        meshData->IndexNum = meshData->mesh->IndicesData.getSize();
        meshData->VertexNum = meshData->IndexNum;
        meshData->VertexArray = new SKINNED_VERTEX_3D[meshData->VertexNum];
        meshData->IndexArray = new unsigned int[meshData->IndexNum];

        for (unsigned int indexCnt = 0; indexCnt < meshData->IndexNum; indexCnt++)
        {
            SKINNED_VERTEX_3D vertex;
            int vertexIndex = meshData->mesh->IndicesData[indexCnt].Index;

            if (vertexIndex < 0)
            {
                vertexIndex = -vertexIndex;
                vertexIndex--;
            }


            vertex.Position = meshData->mesh->VerticesTemp[vertexIndex].Position;
            vertex.Position.z = -vertex.Position.z;
            float BoneIndices[MAX_BONE_INDICES] = { 0.0f, 0.0f , 0.0f , 0.0f };
            float Weights[MAX_BONE_INDICES] = { 1.0f, 0.0f , 0.0f , 0.0f };
            int boneIndicesSize = meshData->mesh->VerticesTemp[vertexIndex].BoneIndices.getSize();
            if (boneIndicesSize > MAX_BONE_INDICES)
            {
                int topWeightsIndices[MAX_BONE_INDICES] = { 0 };
                float topWeights[MAX_BONE_INDICES] = { 0 };
                for (int i = 0; i < boneIndicesSize; ++i)
                {
                    float currentWeight = meshData->mesh->VerticesTemp[vertexIndex].Weights[i];
                    int currentIndex = meshData->mesh->VerticesTemp[vertexIndex].BoneIndices[i];
                    for (int j = 0; j < MAX_BONE_INDICES; ++j)
                    {
                        if (currentWeight > topWeights[j])
                        {
                            for (int k = MAX_BONE_INDICES - 1; k > j; --k)
                            {
                                topWeights[k] = topWeights[k - 1];
                                topWeightsIndices[k] = topWeightsIndices[k - 1];
                            }
                            topWeights[j] = currentWeight;
                            topWeightsIndices[j] = currentIndex;
                            break;
                        }
                    }
                }

                float sumWeights = 0.0f;
                for (int i = 0; i < MAX_BONE_INDICES; i++)
                {
                    sumWeights += topWeights[i];
                }
                if (sumWeights > 0)
                {
                    for (int i = 0; i < MAX_BONE_INDICES; i++)
                    {
                        topWeights[i] /= sumWeights;
                    }
                }

                for (int i = 0; i < MAX_BONE_INDICES; i++)
                {
                    BoneIndices[i] = static_cast<float>(topWeightsIndices[i]);
                    Weights[i] = topWeights[i];

                }
            }
            else
            {
                for (int i = 0; i < boneIndicesSize; i++)
                {
                    BoneIndices[i] = static_cast<float>(meshData->mesh->VerticesTemp[vertexIndex].BoneIndices[i]);
                    Weights[i] = meshData->mesh->VerticesTemp[vertexIndex].Weights[i];

                }
            }

            vertex.BoneIndices = XMFLOAT4(BoneIndices[0], BoneIndices[1], BoneIndices[2], BoneIndices[3]);
            vertex.Weights = XMFLOAT4(Weights[0], Weights[1], Weights[2], Weights[3]);


            if (meshData->normalLoc == Vertex)
                vertex.Normal = meshData->mesh->VerticesTemp[vertexIndex].Normal;
            else if (meshData->normalLoc == Index)
                vertex.Normal = meshData->mesh->IndicesData[indexCnt].Normal;

            if (meshData->texLoc == Vertex)
                vertex.TexCoord = meshData->mesh->VerticesTemp[vertexIndex].TexCoord;
            else if (meshData->texLoc == Index)
            {
                vertex.TexCoord = meshData->mesh->IndicesData[indexCnt].TexCoord;
                vertex.Tangent = meshData->mesh->IndicesData[indexCnt].Tangent;
                vertex.Bitangent = meshData->mesh->IndicesData[indexCnt].Bitangent;
            }

            vertex.TexCoord.y = -vertex.TexCoord.y;

            meshData->VertexArray[indexCnt] = vertex;
            meshData->IndexArray[indexCnt] = indexCnt;

            minPoint.x = min(minPoint.x, vertex.Position.x);
            minPoint.y = min(minPoint.y, vertex.Position.y);
            minPoint.z = min(minPoint.z, vertex.Position.z);

            maxPoint.x = max(maxPoint.x, vertex.Position.x);
            maxPoint.y = max(maxPoint.y, vertex.Position.y);
            maxPoint.z = max(maxPoint.z, vertex.Position.z);
        }

        modelData->numBones = meshData->mBoneHierarchy.getSize();
        modelData->boneTransformData = BoneTransformData(modelData->numBones);
        if (modelData->numBones == 0)
            modelData->numBones = 1;

        int numBones = 1;// model.numBones;
        meshData->boundingBoxes.resize(numBones);
        for (int i = 0; i < numBones; i++)
        {
            if ((i == 0) && (numBones > 1)) continue;

            SKINNED_MESH_BOUNDING_BOX* boundingBox = new SKINNED_MESH_BOUNDING_BOX();

            if (numBones > 1)
                boundingBox->boneIdx = i + 1;
            else
                boundingBox->boneIdx = i;



            boundingBox->maxPoint = maxPoint;
            boundingBox->minPoint = minPoint;
            boundingBox->baseMaxPoint = maxPoint;
            boundingBox->baseMinPoint = minPoint;

            meshData->boundingBoxes.push_back(boundingBox);
        }
    }

    return true;
}

bool FBXLoader::LoadAnimation(SkinnedMeshModel* model, SkinnedMeshModelData* modelData, const char* modelPath, const char* modelName, 
    AnimClipName animName, AnimPlayMode animPlayMode)
{
    char* modelFilename = new char[MODEL_NAME_LENGTH] {};

    strcat(modelFilename, modelPath);

    size_t modelPathLength = strlen(modelPath);
    if (modelPathLength > 0 && modelPath[modelPathLength - 1] != '/' && modelPath[modelPathLength - 1] != '\\')
    {
        strcat(modelFilename, "/");
    }

    strcat(modelFilename, modelName);

    FILE* file;
    file = fopen(modelFilename, "rt");
    if (file == NULL)
    {
        printf("エラー:LoadModel %s \n", modelFilename);
        return false;
    }

    modelData->modelPath = const_cast<char*>(modelPath);
    modelData->modelName = const_cast<char*>(modelName);

    if (modelData->animationClips.search(animName) != nullptr)
        return false;

    FbxNode* rootNode = new FbxNode(modelData->currentRootNodeID);
    modelData->fbxNodes.insert(modelData->currentRootNodeID, rootNode);
    modelData->currentRootNodeID++;
    modelData->currentAnimClip = new AnimationClip();
    modelData->currentAnimClip->SetModel(model);
    modelData->currentAnimClip->name = animName;
    modelData->currentAnimClip->animInfo.playMode = animPlayMode;

    char buffer[1024];
    bool loadSuccess = true;
    long int position = 0;
    while (fgets(buffer, sizeof(buffer), file))
    {
        if (strstr(buffer, "; Object properties"))
        {
            loadSuccess = ParseObjectProperties(file, modelData, true);
        }
        else if (strstr(buffer, "; Object connections"))
        {
            loadSuccess = ParseObjectConnections(file, modelData);
        }

        if (!loadSuccess)
        {
            return false;
        }

        position = ftell(file);
        memset(buffer, 0, sizeof(buffer));
    }

    SimpleArray<FbxNode*> rootNodeChilds;
    for (auto& node : modelData->fbxNodes)
    {
        if (node.key == modelData->currentRootNodeID - 1)
        {
            rootNodeChilds = node.value->childNodes;
        }
    }

    if (rootNodeChilds.getSize() != 0)
    {
        for (UINT i = 0; i < rootNodeChilds.getSize(); i++)
        {
            FbxNode* childNode = rootNodeChilds[i];
            if (childNode->nodeType == FbxNodeType::None
                || childNode->nodeType == FbxNodeType::LimbNode)
            {
                modelData->currentAnimClip->armatureNode = childNode;
                modelData->animationClips.insert(modelData->currentAnimClip->name, *modelData->currentAnimClip);
            }
        }
    }

    return true;
}



bool FBXLoader::ParseObjectDefinitions(FILE* file, SkinnedMeshModelData* modelData)
{
    char buffer[2048];
    bool inDefinitions = false;
    ObjectType objectType = ObjectType::NONE;
    int modelCount = 0, geometryCount = 0, materialCount = 0, textureCount = 0;
    int videoCount = 0, deformerCount = 0, poseCount = 0, globalSettingsCount = 0;
    long int position = 0;
    int bracketCount = 0;
    while (fgets(buffer, sizeof(buffer), file)) 
    {
        char objectTypeBuffer[64];
        if (inDefinitions)
        {
            if (sscanf(buffer, " ObjectType: \"%[^\"]\"", objectTypeBuffer) == 1)
            {
                if (strcmp(objectTypeBuffer, "Model") == 0)
                    objectType = ObjectType::Model;
                else if (strcmp(objectTypeBuffer, "Material") == 0)
                    objectType = ObjectType::Material;
                else if (strcmp(objectTypeBuffer, "Texture") == 0)
                    objectType = ObjectType::Texture;
                else if (strcmp(objectTypeBuffer, "AnimationStack") == 0)
                    objectType = ObjectType::AnimationStack;
                else if (strcmp(objectTypeBuffer, "AnimationLayer") == 0)
                    objectType = ObjectType::AnimationLayer;
                else if (strcmp(objectTypeBuffer, "Geometry") == 0)
                    objectType = ObjectType::Geometry;
                else if (strcmp(objectTypeBuffer, "Implementation") == 0)
                    objectType = ObjectType::Implementation;
                else if (strcmp(objectTypeBuffer, "BindingTable") == 0)
                    objectType = ObjectType::BindingTable;

                int bracket = 0;
                if (strstr(buffer, "{"))
                    bracket++;
                if (strstr(buffer, "}"))
                    bracket--;

                bool inProperty = false;
                switch (objectType)
                {
                case ObjectType::Model:
                    
                    while (fgets(buffer, sizeof(buffer), file))
                    {
                        if (strstr(buffer, "{"))
                            bracket++;
                        if (strstr(buffer, "}"))
                            bracket--;

                        if (bracket == 0)
                            break;

                        if (strstr(buffer, "Properties70:"))
                        {
                            inProperty = true;
                            if (!ParseModelProperty(file, modelData->globalModelProperty, nullptr))
                                return false;
                            bracket--;
                        }
                    }

                    break;
                case ObjectType::Material:
                    while (fgets(buffer, sizeof(buffer), file))
                    {
                        if (strstr(buffer, "{"))
                            bracket++;
                        if (strstr(buffer, "}"))
                            bracket--;

                        if (bracket == 0)
                            break;

                        if (strstr(buffer, "Properties70:"))
                        {
                            inProperty = true;
                            if (!ParseMaterialProperty(file, modelData->globalMaterial))
                                return false;
                            bracket--;
                        }
                    }
                    break;
                default:
                    if (strstr(buffer, "{"))
                        SkipNode(file);
                    break;
                }
            }
            else if (strstr(buffer, "}"))
            {
                return true;
            }


        }
        else if (strstr(buffer, "Definitions:"))
            inDefinitions = true;

            position = ftell(file);

    }

    return false;
}

bool FBXLoader::ParseObjectProperties(FILE* file, SkinnedMeshModelData* modelData, bool loadAnimation)
{
    char buffer[2048];
    bool inObjects = false;

    while (fgets(buffer, sizeof(buffer), file))
    {
        if (inObjects == false)
        {
            if (strchr(buffer, '{'))
            {
                inObjects = true;
            }
        }

        if (inObjects)
        {
            // Geometryフィールドを解析
            if (strstr(buffer, "NodeAttribute"))
            {
                //FbxNode* node = new FbxNode();
                //if (!CreateFbxNode(buffer, node))
                //{
                //    delete node;
                //    return false;
                //}
                //model.fbxNodes.insert(node->nodeID, node);
                SkipNode(file);
            }
            if (strstr(buffer, "Geometry:"))
            {
                if (loadAnimation == true)
                {
                    SkipNode(file);
                    continue;
                }

                FbxNode* node = new FbxNode();
                if (!CreateFbxNode(buffer, node))
                    return false;

                char geometryNameBuffer[128], geometryTypeBuffer[128];
                GeometryType geometryType;
                if (sscanf(buffer, " Geometry: %*[^,], \"%[^\"]\", \"%[^\"]\"", geometryNameBuffer, geometryTypeBuffer) != 2)
                {
                    delete node;
                    return false;   // Geometryの解析が失敗した
                }

                if (strcmp(geometryTypeBuffer, "Mesh") == 0)
                {
                    geometryType = GeometryType::Mesh;
                    if (modelData->meshDataMap.search(node->nodeID) == nullptr)
                    {
                        modelData->meshDataMap.insert(node->nodeID, new MeshData());
                    }
                }
                   
                else if (strcmp(geometryTypeBuffer, "Shape") == 0)
                {
                    geometryType = GeometryType::Shape;

                    //ModelData** ppModelData = model.meshDataMap.search(node->nodeID);
                    //if (ppModelData == nullptr)
                    //{
                    //    model.meshDataMap.insert(node->nodeID, new ModelData());
                    //    ppModelData = model.meshDataMap.search(node->nodeID);
                    //}
                    //
                    //(*ppModelData)->shapeCnt++;
                    //(*ppModelData)->shapes.push_back(MeshData());
                }
                else
                {
                    delete node;
					SkipNode(file);
					continue;   // 未対応のGeometryタイプ
                }

                if (!ParseGeometry(file, modelData, geometryType, node))
                {
                    delete node;
                    return false;   // Geometryの解析が失敗した
                }

                //if ()
                //node->nodeData = static_cast<void*>(model.modelData);

                modelData->fbxNodes.insert(node->nodeID, node);

            }
            // Modelフィールドを解析
            if (strstr(buffer, "Model:"))
            {
                FbxNode* node = new FbxNode();
                if (!CreateFbxNode(buffer, node))
                {
                    delete node;
                    return false;
                }
                

                char modelName[128], modelType[128];
                if (sscanf(buffer, " Model: %*[^,], \"%[^\"]\", \"%[^\"]\"", modelName, modelType) != 2)
                {
                    delete node;
                    return false;   // Modelの解析が失敗した
                }
                if (!ParseModel(file, modelData, node))
                {
                    delete node;
                    return false;   // Modelの解析が失敗した
                }

                modelData->fbxNodes.insert(node->nodeID, node);
            }

            // Poseフィールドを解析
            if (strstr(buffer, "Pose:"))
            {
                FbxNode* node = new FbxNode();
                if (!CreateFbxNode(buffer, node))
                {
                    delete node;
                    return false;
                }
                modelData->fbxNodes.insert(node->nodeID, node);
                
                ParseBindPose(file, modelData, node);
                //SkipNode(file);
            }
            // Materialフィールドを解析
            if (strstr(buffer, "Material:"))
            {
                FbxNode* node = new FbxNode();
                if (!CreateFbxNode(buffer, node))
                {
                    delete node;
                    return false;
                }
                modelData->fbxNodes.insert(node->nodeID, node);

                char materialName[128];
                if (sscanf(buffer, " Material: %*[^,], \"%[^\"]\"", materialName) != 1)
                {
                    delete node;
                    return false;   // Materialの解析が失敗した
                }
                node->nodeType = FbxNodeType::Material;
                if (!ParseMaterial(file, modelData, node))
                {
                    delete node;
                    return false;   // Materialの解析が失敗した
                }
            }
            if (strstr(buffer, "Deformer:"))
            {
                FbxNode* node = new FbxNode();
                if (!CreateFbxNode(buffer, node))
                {
                    delete node;
                    return false;
                }
                    

                if (node->nodeType == FbxNodeType::Skin)
                {
                    Deformer* deformer = new Deformer();
                    deformer->boneIdx = 0;
                    XMStoreFloat4x4(&deformer->Transform, XMMatrixIdentity());
                    XMStoreFloat4x4(&deformer->TransformLink, XMMatrixIdentity());
                    node->nodeData = static_cast<void*>(deformer);
                    SkipNode(file);
                }
                else if (node->nodeType == FbxNodeType::BlendShape
                        || node->nodeType == FbxNodeType::BlendShapeChannel)
                {
                    SkipNode(file);
                }
                else if (!ParseDeformer(file, node))
                {
                    delete node;
                    return false;
                }
                modelData->fbxNodes.insert(node->nodeID, node);
            }
            if (strstr(buffer, "Video:"))
            {
                FbxNode* node = new FbxNode();
                if (!CreateFbxNode(buffer, node))
                {
                    delete node;
                    return false;
                }
                modelData->fbxNodes.insert(node->nodeID, node);

                SkipNode(file);
            }
            // Textureフィールドを解析
            if (strstr(buffer, "Texture:"))
            {
                FbxNode* node = new FbxNode();
                if (!CreateFbxNode(buffer, node))
                {
                    delete node;
                    return false;
                }
                modelData->fbxNodes.insert(node->nodeID, node);

                char TextureName[128];
                if (sscanf(buffer, " Texture: %*[^,], \"%[^\"]\"", TextureName) != 1)
                {
                    delete node;
                    return false;   // Textureの解析が失敗した
                }
                if (!ParseTexture(file, modelData, node))
                {
                    delete node;
                    return false;   // Textureの解析が失敗した
                }

            }
            if (strstr(buffer, "AnimationStack"))
            {
                FbxNode* node = new FbxNode();
                if (!CreateFbxNode(buffer, node))
                {
                    delete node;
                    return false;
                }
                modelData->fbxNodes.insert(node->nodeID, node);

                if (!ParseAnimationStackCurve(file, modelData))
                {
                    delete node;
                    return false;
                }
            }
            if (strstr(buffer, "AnimationCurve"))
            {
                FbxNode* node = new FbxNode();
                if (!CreateFbxNode(buffer, node))
                {
                    delete node;
                    return false;
                }
                modelData->fbxNodes.insert(node->nodeID, node);

                if (!ParseAnimationCurve(file, node))
                {
                    delete node;
                    return false;
                }
            }
            if (strstr(buffer, "AnimationCurveNode"))
            {
                FbxNode* node = new FbxNode();
                if (!CreateFbxNode(buffer, node))
                {
                    delete node;
                    return false;
                }
                modelData->fbxNodes.insert(node->nodeID, node);

                SkipNode(file);
            }
        }

        if (strstr(buffer, "}"))
        {
            return true;
        }

        memset(buffer, 0, sizeof(buffer));
    }

    return false;
}

bool FBXLoader::ParseGeometry(FILE* file, SkinnedMeshModelData* modelData, GeometryType type, FbxNode* node)
{
    char buffer[2048];
    bool readVertice = false;
    long int position = 0;
    ModelMeshData* modelMeshData = nullptr;
    SimpleArray<float> normals;
    // Geometry内部のフィールドを処理する
    while (fgets(buffer, sizeof(buffer), file))
    {
        if (strstr(buffer, "Vertices:"))
        {
            if (!ParseVertexData(file, modelData, type, node, modelMeshData))
                return false;
        }
        else if (strstr(buffer, "PolygonVertexIndex:")
                || strstr(buffer, "Indexes:"))
        {
            if (!ParseIndexData(file, modelData, type, node, modelMeshData))
                return false;
        }
        else if (strstr(buffer, "LayerElementNormal:"))
        {
            if (!ParseNormal(file, modelData, type, node, modelMeshData))
                return false;     
        }
        else if (strstr(buffer, "Normals: "))
        {
            if (!ParseNormalData(file, modelData, type, MappingInformationType::ByPolygonVertex, ReferenceInformationType::Direct, node, modelMeshData, normals))
                return false;

        }
        else if (strstr(buffer, "Edges: "))
        {
            SkipNode(file);

        }
        else if (strstr(buffer, "LayerElementTangent:"))
        {
            //fseek(file, position, SEEK_SET);
            SkipNode(file);
        }
        else if (strstr(buffer, "LayerElementColor:"))
        {
            //fseek(file, position, SEEK_SET);
            SkipNode(file);
        }
        else if (strstr(buffer, "LayerElementSmoothing:"))
        {
            while (!strstr(buffer, "}"))
                fgets(buffer, sizeof(buffer), file);
        }
        else if (strstr(buffer, "LayerElementUV:"))
        {
            if (!ParseUV(file, modelData, type, node, modelMeshData))
                return false;
        }
        else if (strstr(buffer, "LayerElementMaterial:"))
        {
            //fseek(file, position, SEEK_SET);
            SkipNode(file);
        }
        else if (strstr(buffer, "Layer:"))
        {
            SkipNode(file);
        }
        else if (strstr(buffer, "}"))
        {
            // Geometryセグメントの終わり
            if (type == GeometryType::Shape)
            {
                if (modelMeshData == nullptr)
                    return false;

                node->nodeData = static_cast<void*>(modelMeshData);
            }
            return true;
        }

        memset(buffer, 0, sizeof(buffer));

        position = ftell(file);
    }

    return false;  // ファイルが意外に終了した場合
}

bool FBXLoader::ParseModel(FILE* file, SkinnedMeshModelData* modelData, FbxNode* node)
{
    char buffer[2048];
    int readCount;
    long int position = 0;

    while (fgets(buffer, sizeof(buffer), file))
    {
        if (strstr(buffer, "Properties70:"))
        {
            if (!ParseModelProperty(file, modelData->modelProperty, node))
                return false;
        }
        else if (strstr(buffer, "Culling:"))
        {
            char culling[32];

            readCount = sscanf(buffer, " Culling: \"%[^\"]\"", culling);
            if (readCount != 1)
                return false;
            if (strcmp(culling, "CullingOff") == 0)
                modelData->cullMode = CULL_MODE_NONE;
        }
        else if (strstr(buffer, "}"))
        {
            // Modelセグメントの終わり
            modelData->ModelCount++;
            return true;
        }

        memset(buffer, 0, sizeof(buffer));
        position = ftell(file);
    }

    return false;
}

bool FBXLoader::ParseMaterial(FILE* file, SkinnedMeshModelData* modelData, FbxNode* node)
{
    char buffer[2048];
    long int position = 0;
    FbxMaterial* material = new FbxMaterial();
    // Material内部のフィールドを処理する
    while (fgets(buffer, sizeof(buffer), file))
    {
        if (strstr(buffer, "Properties70:"))
        {
            if (!ParseMaterialProperty(file, *material))
            {
                delete material;
                return false;
            }
        }

        if (strstr(buffer, "}"))
        {
            node->nodeData = static_cast<void*>(material);
            return true;
        }
    }
    delete material;
    return false;
}

bool FBXLoader::ParseDeformer(FILE* file, FbxNode* node)
{
    char buffer[4096];
    char* ptr;
    int index;
    int indexCount = 0;
    float weight;
    int weightCount = 0;
    Deformer* deformer = new Deformer;
    while (fgets(buffer, sizeof(buffer), file))
    {
        if (sscanf(buffer, " Indexes: *%d", &indexCount) == 1)
        {
            int IndexFound = 0;
            while (fgets(buffer, sizeof(buffer), file))
            {
                ptr = buffer;
                if (!IndexFound)
                {
                    ptr = strstr(buffer, "a:");
                    if (ptr)
                    {
                        deformer->Index.resize(indexCount);
                        ptr += strlen("a:");
                        IndexFound = 1;
                    }
                }

                if (IndexFound)
                {
                    while (*ptr && isspace((unsigned char)*ptr)) ptr++;

                    if (!isdigit((unsigned char)*ptr) && *ptr != '-' && *ptr != '.')
                        break;

                    while (sscanf(ptr, "%d", &index) == 1)
                    {
                        deformer->Index.push_back(index);

                        while (*ptr != ',' && *ptr != '\0') ptr++;
                        if (*ptr == ',') ptr++;
                    }
                }
            }
        }
        else if (sscanf(buffer, " Weights: *%d", &weightCount) == 1)
        {
            int WeightFound = 0;
            while (fgets(buffer, sizeof(buffer), file))
            {
                ptr = buffer;
                if (!WeightFound)
                {
                    ptr = strstr(buffer, "a:");
                    if (ptr)
                    {
                        deformer->Weights.resize(weightCount);
                        ptr += strlen("a:");
                        WeightFound = 1;
                    }
                }

                if (WeightFound)
                {
                    while (*ptr && isspace((unsigned char)*ptr)) ptr++;

                    if (!isdigit((unsigned char)*ptr) && *ptr != '-' && *ptr != '.')
                        break;

                    while (sscanf(ptr, "%f", &weight) == 1)
                    {
                        deformer->Weights.push_back(weight);

                        while (*ptr != ',' && *ptr != '\0') ptr++;
                        if (*ptr == ',') ptr++;
                    }
                }
            }
        }
        else if (strstr(buffer, "Transform:"))
        {
            int TransformFound = 0;
            SimpleArray<float> transformArray;
            while (fgets(buffer, sizeof(buffer), file))
            {
                ptr = buffer;
                if (!TransformFound)
                {
                    ptr = strstr(buffer, "a:");
                    if (ptr)
                    {
                        ptr += strlen("a:");
                        TransformFound = 1;
                    }
                }

                if (TransformFound)
                {
                    while (*ptr && isspace((unsigned char)*ptr)) ptr++;

                    if (!isdigit((unsigned char)*ptr) && *ptr != '-' && *ptr != '.')
                        break;

                    while (sscanf(ptr, "%f", &weight) == 1)
                    {
                        transformArray.push_back(weight);

                        while (*ptr != ',' && *ptr != '\0') ptr++;
                        if (*ptr == ',') ptr++;
                    }
                }
            }
            if (transformArray.getSize() != 16)
            {
                delete deformer;
                return false;
            }
                
            int index = 0;
            for (int col = 0; col < 4; col++) 
            {
                for (int row = 0; row < 4; row++) 
                {
                    deformer->Transform.m[col][row] = transformArray[index++];
                }
            }
        }
        else if (strstr(buffer, "TransformLink:"))
        {
            int TransformLinkFound = 0;
            SimpleArray<float> transformArrayLink;
            while (fgets(buffer, sizeof(buffer), file))
            {
                ptr = buffer;
                if (!TransformLinkFound)
                {
                    ptr = strstr(buffer, "a:");
                    if (ptr)
                    {
                        ptr += strlen("a:");
                        TransformLinkFound = 1;
                    }
                }

                if (TransformLinkFound)
                {
                    while (*ptr && isspace((unsigned char)*ptr)) ptr++;

                    if (!isdigit((unsigned char)*ptr) && *ptr != '-' && *ptr != '.')
                        break;

                    while (sscanf(ptr, "%f", &weight) == 1)
                    {
                        transformArrayLink.push_back(weight);

                        while (*ptr != ',' && *ptr != '\0') ptr++;
                        if (*ptr == ',') ptr++;
                    }
                }
            }
            if (transformArrayLink.getSize() != 16)
            {
                delete deformer;
                return false;
            }
            int index = 0;
            for (int col = 0; col < 4; col++)
            {
                for (int row = 0; row < 4; row++)
                {
                    deformer->TransformLink.m[col][row] = transformArrayLink[index++];
                }
            }
        }
        else if (strstr(buffer, "}"))
        {
            if (deformer->Weights.getSize() == weightCount &&
                deformer->Index.getSize() == indexCount)
            {
                node->nodeData = static_cast<void*>(deformer);
            }
            else
            {
                delete deformer;
            }
                
            return true;
        }
            
    }
    return false;
}

bool FBXLoader::ParseMaterialProperty(FILE* file, FbxMaterial& material)
{
    char buffer[2048];
    char property[128] = { 0 };
    int readCount = 0;
    while (fgets(buffer, sizeof(buffer), file))
    {
        sscanf(buffer, " P: \"%[^\"]\"", property);

        if (strstr(property, "MultiLayer"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &material.MultiLayer);
            if (readCount != 1)
                return false;
        }
        else if (strstr(property, "EmissiveColor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &material.EmissiveColor.x, &material.EmissiveColor.y, &material.EmissiveColor.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(property, "EmissiveFactor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f", &material.EmissiveFactor);
            if (readCount != 1)
                return false;
        }
        else if (strstr(property, "AmbientColor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &material.AmbientColor.x, &material.AmbientColor.y, &material.AmbientColor.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(property, "AmbientFactor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f", &material.AmbientFactor);
            if (readCount != 1)
                return false;
        }
        else if (strstr(property, "DiffuseColor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &material.DiffuseColor.x, &material.DiffuseColor.y, &material.DiffuseColor.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(property, "DiffuseFactor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f", &material.DiffuseFactor);
            if (readCount != 1)
                return false;
        }
        else if (strstr(property, "Bump\""))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &material.Bump.x, &material.Bump.y, &material.Bump.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(property, "NormalMap"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &material.NormalMap.x, &material.NormalMap.y, &material.NormalMap.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(property, "BumpFactor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &material.BumpFactor);
            if (readCount != 1)
                return false;
        }
        else if (strstr(property, "TransparentColor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &material.TransparentColor.x, &material.TransparentColor.y, &material.TransparentColor.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(property, "TransparencyFactor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &material.TransparencyFactor);
            if (readCount != 1)
                return false;
        }
        else if (strstr(property, "DisplacementColor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &material.DisplacementColor.x, &material.DisplacementColor.y, &material.DisplacementColor.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(property, "DisplacementFactor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &material.DisplacementFactor);
            if (readCount != 1)
                return false;
        }
        else if (strstr(property, "VectorDisplacementColor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &material.VectorDisplacementColor.x, &material.VectorDisplacementColor.y, &material.VectorDisplacementColor.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(property, "VectorDisplacementFactor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &material.VectorDisplacementFactor);
            if (readCount != 1)
                return false;
        }
        else if (strstr(property, "SpecularColor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &material.SpecularColor.x, &material.SpecularColor.y, &material.SpecularColor.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(property, "SpecularFactor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f", &material.SpecularFactor);
            if (readCount != 1)
                return false;
        }
        else if (strstr(property, "ShininessExponent"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f", &material.ShininessExponent);
            if (readCount != 1)
                return false;
        }
        else if (strstr(property, "ReflectionColor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &material.ReflectionColor.x, &material.ReflectionColor.y, &material.ReflectionColor.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(property, "ReflectionFactor"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f", &material.ReflectionFactor);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "}"))
        {
            // MaterialPropertyセグメントの終わり
            return true;
        }

        memset(property, 0, sizeof(property));
    }
    return false;
}

bool FBXLoader::ParseModelProperty(FILE* file, ModelProperty& modelProperty, FbxNode* node)
{
    char buffer[2048];
    int readCount;
    int propertyCnt = 0;
    long int pos = 0;
    while (fgets(buffer, sizeof(buffer), file))
    {
        if (strstr(buffer, "QuaternionInterpolate"))
        {
            readCount = sscanf(buffer, " %*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.QuaternionInterpolate);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "RotationOffset"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.RotationOffset.x, &modelProperty.RotationOffset.y, &modelProperty.RotationOffset.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "RotationPivot"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.RotationPivot.x, &modelProperty.RotationPivot.y, &modelProperty.RotationPivot.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "ScalingOffset"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.ScalingOffset.x, &modelProperty.ScalingOffset.y, &modelProperty.ScalingOffset.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "ScalingPivot"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.ScalingPivot.x, &modelProperty.ScalingPivot.y, &modelProperty.ScalingPivot.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "TranslationActive"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.TranslationActive);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "TranslationMin\""))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.TranslationMin.x, &modelProperty.TranslationMin.y, &modelProperty.TranslationMin.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "TranslationMax\""))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.TranslationMax.x, &modelProperty.TranslationMax.y, &modelProperty.TranslationMax.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "TranslationMinX"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.TranslationMinX);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "TranslationMinY"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.TranslationMinY);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "TranslationMinZ"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.TranslationMinZ);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "TranslationMaxX"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.TranslationMaxX);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "TranslationMaxY"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.TranslationMaxY);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "TranslationMaxZ"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.TranslationMaxZ);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "RotationOrder"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.RotationOrder);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "RotationSpaceForLimitOnly"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.RotationSpaceForLimitOnly);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "RotationStiffnessX"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.RotationStiffnessX);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "RotationStiffnessY"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.RotationStiffnessY);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "RotationStiffnessZ"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.RotationStiffnessZ);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "AxisLen"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.AxisLen);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "PreRotation"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.PreRotation.x, &modelProperty.PreRotation.y, &modelProperty.PreRotation.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "PostRotation"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.PostRotation.x, &modelProperty.PostRotation.y, &modelProperty.PostRotation.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "RotationActive"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.RotationActive);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "RotationMin\""))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.RotationMin.x, &modelProperty.RotationMin.y, &modelProperty.RotationMin.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "RotationMax\""))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.RotationMax.x, &modelProperty.RotationMax.y, &modelProperty.RotationMax.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "RotationMinX"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.RotationMinX);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "RotationMinY"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.RotationMinY);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "RotationMinZ"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.RotationMinZ);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "RotationMaxX"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.RotationMaxX);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "RotationMaxY"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.RotationMaxY);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "RotationMaxZ"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.RotationMaxZ);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "InheritType"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.InheritType);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "ScalingActive"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.ScalingActive);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "ScalingMin\""))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.ScalingMin.x, &modelProperty.ScalingMin.y, &modelProperty.ScalingMin.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "ScalingMax\""))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.ScalingMax.x, &modelProperty.ScalingMax.y, &modelProperty.ScalingMax.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "ScalingMinX"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.ScalingMinX);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "ScalingMinY"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.ScalingMinY);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "ScalingMinZ"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.ScalingMinZ);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "ScalingMaxX"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.ScalingMaxX);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "ScalingMaxY"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.ScalingMaxY);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "ScalingMaxZ"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.ScalingMaxZ);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "GeometricTranslation"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.GeometricTranslation.x, &modelProperty.GeometricTranslation.y, &modelProperty.GeometricTranslation.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "GeometricRotation"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.GeometricRotation.x, &modelProperty.GeometricRotation.y, &modelProperty.GeometricRotation.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "GeometricScaling"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.GeometricScaling.x, &modelProperty.GeometricScaling.y, &modelProperty.GeometricScaling.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "MinDampRangeX"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.MinDampRangeX);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "MinDampRangeY"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.MinDampRangeY);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "MinDampRangeZ"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.MinDampRangeZ);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "MaxDampRangeX"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.MaxDampRangeX);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "MaxDampRangeY"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.MaxDampRangeY);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "MaxDampRangeZ"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.MaxDampRangeZ);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "MinDampStrengthX"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.MinDampStrengthX);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "MinDampStrengthY"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.MinDampStrengthY);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "MinDampStrengthZ"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.MinDampStrengthZ);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "MaxDampStrengthX"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.MaxDampStrengthX);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "MaxDampStrengthY"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.MaxDampStrengthY);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "MaxDampStrengthZ"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.MaxDampStrengthZ);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "PreferedAngleX"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.PreferedAngleX);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "PreferedAngleY"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.PreferedAngleY);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "PreferedAngleZ"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%lf", &modelProperty.PreferedAngleZ);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "Show"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.Show);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "NegativePercentShapeSupport"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.NegativePercentShapeSupport);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "DefaultAttributeIndex"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.DefaultAttributeIndex);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "Freeze"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.Freeze);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "LODBox"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.LODBox);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "Lcl Translation"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.Translation.x, &modelProperty.Translation.y, &modelProperty.Translation.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "Lcl Rotation"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.Rotation.x, &modelProperty.Rotation.y, &modelProperty.Rotation.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "Lcl Scaling"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%f, %f, %f", &modelProperty.Scaling.x, &modelProperty.Scaling.y, &modelProperty.Scaling.z);
            if (readCount != 3)
                return false;
        }
        else if (strstr(buffer, "Visibility"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.Visibility);
            if (readCount != 1)
                return false;
        }
        else if (strstr(buffer, "Visibility Inheritance"))
        {
            readCount = sscanf(buffer, "%*[^,],%*[^,],%*[^,],%*[^,],%d", &modelProperty.VisibilityInheritance);
            if (readCount != 1)
                return false;
        }

        
        if (strstr(buffer, "}"))
        {
            // ModelPropertyセグメントの終わり
            if (node)
            {
                ModelProperty* property = new ModelProperty(modelProperty);
                modelProperty.Clear();
                node->nodeData = static_cast<void*>(property);
            }
                
            return true;
        }

        memset(buffer, 0, sizeof(buffer));
        pos = ftell(file);
    }

    return true;
}

bool FBXLoader::ParseVertexData(FILE* file, SkinnedMeshModelData* modelData, GeometryType geometryType, FbxNode* node, ModelMeshData* &modelMeshData)
{
    char buffer[4096];
    float vertexPos[3];
    float vertex;
    int count = 0;
    char* ptr;
    int verticesFound = 0;
    long int position = 0;
    while (fgets(buffer, sizeof(buffer), file))
    {
        ptr = buffer;
        if (!verticesFound) 
        {
            ptr = strstr(buffer, "a:");
            if (ptr) 
            {
                ptr += strlen("a:");
                verticesFound = 1;
            }
        }

        if (verticesFound)
        {
            while (*ptr && isspace((unsigned char)*ptr)) ptr++;

            if (!isdigit((unsigned char)*ptr) && *ptr != '-' && *ptr != '.' && *ptr != ',')
                break;

            while (sscanf(ptr, "%f", &vertex) == 1) 
            {
                if (count < 3) 
                {
                    vertexPos[count++] = vertex;
                }
                if (count == 3) 
                {
                    SKINNED_VERTEX_3D_TEMP newVertex;
                    newVertex.Position.x = vertexPos[0];
                    newVertex.Position.y = vertexPos[1];
                    newVertex.Position.z = vertexPos[2];
                    if (geometryType == GeometryType::Mesh)
                    {
                        MeshData* meshData = modelData->meshDataMap[node->nodeID];
                        if (meshData == nullptr)
                            meshData = new MeshData();
                        meshData->mesh->VerticesTemp.push_back(newVertex);
                    }
                        
                    else if (geometryType == GeometryType::Shape)
                    {
                        if (modelMeshData == nullptr)
                            modelMeshData = new ModelMeshData();
                        modelMeshData->VerticesTemp.push_back(newVertex);
                    }
                        

                    count = 0;
                }
                
                while (*ptr != ',' && *ptr != '\0') ptr++;
                if (*ptr == ',') ptr++;
            }
        }

        position = ftell(file);
    }

    if (count > 0 && count < 3)
    {
        if (modelMeshData)
            delete modelMeshData;

        return false;
    }
    else
    {
        if (!strstr(buffer, "}"))
            fseek(file, position, SEEK_SET);
        return true;
    }

    return true;
}

bool FBXLoader::ParseIndexData(FILE* file, SkinnedMeshModelData* modelData, GeometryType geometryType, FbxNode* node, ModelMeshData* &modelMeshData)
{
    char buffer[4096];
    int index;
    int count = 0;
    char* ptr;
    int indicesFound = 0;
    long int position = 0;

    MeshData* meshData = nullptr;
    if (node->nodeType == FbxNodeType::Mesh)
    {
        meshData = modelData->meshDataMap[node->nodeID];
        if (meshData == nullptr)
            meshData = new MeshData();
    }


    while (fgets(buffer, sizeof(buffer), file))
    {
        ptr = buffer;
        if (!indicesFound)
        {
            ptr = strstr(buffer, "a:");
            if (ptr)
            {
                ptr += strlen("a:");
                indicesFound = 1;
            }
        }

        if (indicesFound)
        {
            while (*ptr && isspace((unsigned char)*ptr)) ptr++;

            if (!isdigit((unsigned char)*ptr) && *ptr != '-' && *ptr != '.') break;

            int polygonSize = 4;
            while (sscanf(ptr, "%d", &index) == 1)
            {
                IndexData indexData;
                indexData.Index = index;
                if (count == 2 && index < 0)
                    polygonSize = 3;
                
                if (geometryType == GeometryType::Mesh)
                {
                    meshData->mesh->IndicesData.push_back(indexData);
                }
                else if (geometryType == GeometryType::Shape)
                {
                    if (modelMeshData == nullptr)
                        modelMeshData = new ModelMeshData();
                    modelMeshData->IndicesData.push_back(indexData);
                }

                count++;

                if (count == polygonSize)
                    count = 0;

                while (*ptr != ',' && *ptr != '\0') ptr++;
                if (*ptr == ',') ptr++;
            }
        }

        position = ftell(file);
    }

    if (count > 0 && count < 4 && geometryType == GeometryType::Mesh)
    {
       meshData->mesh->IndicesData.clear();

        if (meshData)
            delete meshData;

        return false;
    }
    else
    {
        if (!strstr(buffer, "}"))
            fseek(file, position, SEEK_SET);
        return true;
    }
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             }

bool FBXLoader::ParseNormal(FILE* file, SkinnedMeshModelData* modelData, GeometryType geometryType, FbxNode* node, ModelMeshData* &modelMeshData)
{
    char buffer[4096];
    int readCount;
    char MappingInformationTypeBuffer[32];
    char ReferenceInformationTypeBuffer[32];
    int normalFound = 0;
    int position;
    MappingInformationType mappingInformationType{};
    ReferenceInformationType referenceInformationType{};
    SimpleArray<float> normals;

    while (fgets(buffer, sizeof(buffer), file))
    {

        if (strstr(buffer, "MappingInformationType:"))
        {
            readCount = sscanf(buffer, " MappingInformationType: \"%[^\"]\"", MappingInformationTypeBuffer);
            if (readCount != 1)
                return false;

            if (strcmp(MappingInformationTypeBuffer, "ByPolygon") == 0)
                mappingInformationType = ByPolygon;
            else if (strcmp(MappingInformationTypeBuffer, "ByPolygonVertex") == 0)
                mappingInformationType = ByPolygonVertex;
            else if (strcmp(MappingInformationTypeBuffer, "ByVertice") == 0)
                mappingInformationType = ByVertice;
            else if (strcmp(MappingInformationTypeBuffer, "ByEdge") == 0)
                mappingInformationType = ByEdge;
            else if (strcmp(MappingInformationTypeBuffer, "AllSame") == 0)
                mappingInformationType = AllSame;
            else
                return false;

            if (mappingInformationType == ByVertice)
            {
                MeshData* meshData = modelData->meshDataMap[node->nodeID];
                if (meshData == nullptr)
                    meshData = new MeshData();
                meshData->normalLoc = Vertex;
            }   
            else if (mappingInformationType == ByPolygonVertex)
            {
                MeshData* meshData = modelData->meshDataMap[node->nodeID];
                if (meshData == nullptr)
                    meshData = new MeshData();
                meshData->normalLoc = Index;
            }
                
        }
        else if (strstr(buffer, "ReferenceInformationType:"))
        {
            readCount = sscanf(buffer, " ReferenceInformationType: \"%[^\"]\"", ReferenceInformationTypeBuffer);
            if (readCount != 1)
                return false;

            if (strcmp(ReferenceInformationTypeBuffer, "Direct") == 0)
                referenceInformationType = Direct;
            else if (strcmp(ReferenceInformationTypeBuffer, "IndexToDirect") == 0)
                referenceInformationType = IndexToDirect;
            else
                return false;

        }
        else if (strstr(buffer, "Normals:"))
        {
            if (!ParseNormalData(file, modelData, geometryType, mappingInformationType, referenceInformationType, node, modelMeshData, normals))
                return false;
        }
        else if (strstr(buffer, "NormalsW:"))
            SkipNode(file);
        else if (strstr(buffer, "NormalsIndex:"))
        {
            if (!ParseNormalIndexData(file, modelData, geometryType, node, modelMeshData, normals))
                return false;
        }

        else if (strstr(buffer, "}"))
        {
            // Modelセグメントの終わり
            //fgets(buffer, sizeof(buffer), file);
            return true;
        }

        position = ftell(file);
    }

    return false;
}

bool FBXLoader::ParseNormalData(FILE* file, SkinnedMeshModelData* modelData, GeometryType geometryType,
    MappingInformationType mappingInformationType, ReferenceInformationType referenceInformationType, FbxNode* node, ModelMeshData* &modelMeshData,
    SimpleArray<float>& normals)
{
    char buffer[4096];
    float normal;
    UINT normalCount = 0;
    UINT totalNormalCount = 0;
    UINT vertexCount = 0;
    int normalFound = 0;
    float normalsByVertex[3];
    char* ptr;
    long int position = 0;

    MeshData* meshData = nullptr;
    if (node->nodeType == FbxNodeType::Mesh)
    {
        meshData = modelData->meshDataMap[node->nodeID];
        if (meshData == nullptr)
            meshData = new MeshData();
    }

    while (fgets(buffer, sizeof(buffer), file))
    {
        ptr = buffer;
        if (!normalFound)
        {
            ptr = strstr(buffer, "a:");
            if (ptr)
            {
                ptr += strlen("a:");
                normalFound = 1;
            }
        }

        if (normalFound)
        {
            while (*ptr && isspace((unsigned char)*ptr)) ptr++;

            if (!isdigit((unsigned char)*ptr) && *ptr != '-' && *ptr != '.') break;

            if (mappingInformationType == ByPolygonVertex
                && referenceInformationType == Direct)
            {

                while (sscanf(ptr, "%f", &normal) == 1)
                {
                    if (normalCount < 3)
                    {
                        normalsByVertex[normalCount % 3] = normal;
                        normalCount++;
                        totalNormalCount++;
                    }
                    if (normalCount == 3)
                    {
                        if (geometryType == GeometryType::Mesh)
                        {

                            MeshData* meshData = modelData->meshDataMap[node->nodeID];
                            if (meshData == nullptr)
                                meshData = new MeshData();
                            meshData->mesh->IndicesData[vertexCount].Normal.x = normalsByVertex[0];
                            meshData->mesh->IndicesData[vertexCount].Normal.y = normalsByVertex[1];
                            meshData->mesh->IndicesData[vertexCount].Normal.z = normalsByVertex[2];
                        }
                        else if (geometryType == GeometryType::Shape)
                        {

                            if (modelMeshData == nullptr)
                            {
                                modelMeshData = new ModelMeshData();

                                IndexData indexData;
                                indexData.Normal.x = normalsByVertex[0];
                                indexData.Normal.y = normalsByVertex[1];
                                indexData.Normal.z = normalsByVertex[2];
                                modelMeshData->IndicesData.push_back(indexData);
                            }
                            else
                            {
                                modelMeshData->IndicesData[vertexCount].Normal.x = normalsByVertex[0];
                                modelMeshData->IndicesData[vertexCount].Normal.y = normalsByVertex[1];
                                modelMeshData->IndicesData[vertexCount].Normal.z = normalsByVertex[2];
                            }
                                

                        }

                        vertexCount++;
                        normalCount = 0;
                    }

                    while (*ptr != ',' && *ptr != '\0') ptr++;
                    if (*ptr == ',') ptr++;
                }
            }
            else if (mappingInformationType == ByVertice
                && referenceInformationType == Direct)
            {
                while (sscanf(ptr, "%f", &normal) == 1)
                {
                    if (normalCount < 3)
                    {
                        normalsByVertex[normalCount % 3] = normal;
                        normalCount++;
                        totalNormalCount++;
                    }
                    if (normalCount == 3)
                    {
                        if (geometryType == GeometryType::Mesh)
                        {
                            if (vertexCount >= meshData->mesh->VerticesTemp.getSize())
                            {
                                delete meshData;
                                return false;
                            }
                            meshData->mesh->VerticesTemp[vertexCount].Normal.x = normalsByVertex[0];
                            meshData->mesh->VerticesTemp[vertexCount].Normal.y = normalsByVertex[1];
                            meshData->mesh->VerticesTemp[vertexCount].Normal.z = normalsByVertex[2];
                        }
                        else if (geometryType == GeometryType::Shape)
                        {
                            if (modelMeshData == nullptr)
                                return false;
                            if (vertexCount >= modelMeshData->VerticesTemp.getSize())
                            {
                                delete modelMeshData;
                                return false;
                            }
                            modelMeshData->VerticesTemp[vertexCount].Normal.x = normalsByVertex[0];
                            modelMeshData->VerticesTemp[vertexCount].Normal.y = normalsByVertex[1];
                            modelMeshData->VerticesTemp[vertexCount].Normal.z = normalsByVertex[2];
                        }

                        vertexCount++;
                        normalCount = 0;
                    }

                    while (*ptr != ',' && *ptr != '\0') ptr++;
                    if (*ptr == ',') ptr++;
                }
            }
            else if (mappingInformationType == ByPolygonVertex
                && referenceInformationType == IndexToDirect)
                {
                    while (sscanf(ptr, "%f", &normal) == 1)
                    {
                        normals.push_back(normal);
                        while (*ptr != ',' && *ptr != '\0') ptr++;
                        if (*ptr == ',') ptr++;
                    }
                }
        }

        position = ftell(file);
    }

    int VertexArrayCnt = 0, IndexArrayCnt = 0;
    if (geometryType == GeometryType::Mesh)
    {
        VertexArrayCnt = meshData->mesh->Vertices.getSize();
        IndexArrayCnt = meshData->mesh->IndicesData.getSize();
    }
    else if (geometryType == GeometryType::Shape)
    {
        if (modelMeshData == nullptr)
            return false;
        VertexArrayCnt = modelMeshData->Vertices.getSize();
        IndexArrayCnt = modelMeshData->IndicesData.getSize();
    }


    if (mappingInformationType == ByPolygon
        && referenceInformationType == Direct
        && vertexCount != VertexArrayCnt)
    {
        if (modelMeshData)
            delete modelMeshData;

        return false;
    }
    else if (mappingInformationType == ByPolygonVertex
        && referenceInformationType == Direct
        && vertexCount != IndexArrayCnt)
    {
        if (modelMeshData)
            delete modelMeshData;

        return false;
    }

    return true;
}

bool FBXLoader::ParseNormalIndexData(FILE* file, SkinnedMeshModelData* modelData, GeometryType geometryType, FbxNode* node, ModelMeshData*& modelMeshData, SimpleArray<float>& normals)
{
    if (normals.getSize() <= 0)
        return false;

    char buffer[4096];
    SimpleArray<int> normalIndex;
    int index;
    int normalIndexFound = 0;
    char* ptr;

    MeshData* meshData = nullptr;
    if (node->nodeType == FbxNodeType::Mesh)
    {
        meshData = modelData->meshDataMap[node->nodeID];
        if (meshData == nullptr)
            meshData = new MeshData();
    }

    while (fgets(buffer, sizeof(buffer), file))
    {
        ptr = buffer;
        if (!normalIndexFound)
        {
            ptr = strstr(buffer, "a:");
            if (ptr)
            {
                ptr += strlen("a:");
                normalIndexFound = 1;
            }
        }

        if (normalIndexFound)
        {
            while (*ptr && isspace((unsigned char)*ptr)) ptr++;

            if (!isdigit((unsigned char)*ptr) && *ptr != '-' && *ptr != '.') 
                break;

            while (sscanf(ptr, "%d", &index) == 1)
            {
                normalIndex.push_back(index);

                while (*ptr != ',' && *ptr != '\0') ptr++;
                if (*ptr == ',') ptr++;
            }
        }
    }

    int normalIndexSize = normalIndex.getSize();

    for (int i = 0; i < normalIndexSize; i++)
    {
        if (normalIndexSize != meshData->mesh->IndicesData.getSize())
            return false;

        int index = normalIndex[i];

        meshData->mesh->IndicesData[i].Normal.x = normals[index * 3];
        meshData->mesh->IndicesData[i].Normal.y = normals[index * 3 + 1];
        meshData->mesh->IndicesData[i].Normal.z = normals[index * 3 + 2];
    }

    return true;
}

bool FBXLoader::ParseUV(FILE* file, SkinnedMeshModelData* modelData, GeometryType geometryType, FbxNode* node, ModelMeshData* &modelMeshData)
{
    char buffer[4096];
    int readCount;
    char MappingInformationTypeBuffer[32];
    char ReferenceInformationTypeBuffer[32];
    char* ptr;
    SimpleArray<float> UVCoord;
    SimpleArray<UINT> UVCoordIndex;
    float UV;
    int UVIndex;
    long int position = 0;
    MappingInformationType mappingInformationType;
    ReferenceInformationType referenceInformationType;

    MeshData* meshData = nullptr;
    if (geometryType == GeometryType::Mesh)
    {
        meshData = modelData->meshDataMap[node->nodeID];
        if (meshData == nullptr)
            return false;
    }

    while (fgets(buffer, sizeof(buffer), file))
    {

        if (strstr(buffer, "MappingInformationType:"))
        {
            readCount = sscanf(buffer, " MappingInformationType: \"%[^\"]\"", MappingInformationTypeBuffer);
            if (readCount != 1)
                return false;

            if (strcmp(MappingInformationTypeBuffer, "ByPolygon") == 0)
                mappingInformationType = ByPolygon;
            else if (strcmp(MappingInformationTypeBuffer, "ByPolygonVertex") == 0)
                mappingInformationType = ByPolygonVertex;
            else if (strcmp(MappingInformationTypeBuffer, "ByVertice") == 0)
                mappingInformationType = ByVertice;
            else if (strcmp(MappingInformationTypeBuffer, "ByEdge") == 0)
                mappingInformationType = ByEdge;
            else if (strcmp(MappingInformationTypeBuffer, "AllSame") == 0)
                mappingInformationType = AllSame;
            else
                return false;

            if (mappingInformationType == ByVertice)
            {
                MeshData* meshData = modelData->meshDataMap[node->nodeID];
                if (meshData == nullptr)
                    meshData = new MeshData();
                meshData->texLoc = Vertex;
            }
                
            else if (mappingInformationType == ByPolygonVertex)
            {
                MeshData* meshData = modelData->meshDataMap[node->nodeID];
                if (meshData == nullptr)
                    meshData = new MeshData();
                meshData->texLoc = Index;
            }
                
        }
        else if (strstr(buffer, "ReferenceInformationType:"))
        {
            readCount = sscanf(buffer, " ReferenceInformationType: \"%[^\"]\"", ReferenceInformationTypeBuffer);
            if (readCount != 1)
                return false;

            if (strcmp(ReferenceInformationTypeBuffer, "Direct") == 0)
                referenceInformationType = Direct;
            else if (strcmp(ReferenceInformationTypeBuffer, "IndexToDirect") == 0)
                referenceInformationType = IndexToDirect;
            else
                return false;

        }
        else if (strstr(buffer, "UV:"))
        {
            int UVFound = 0;
            while (fgets(buffer, sizeof(buffer), file))
            {
                ptr = buffer;
                if (!UVFound)
                {
                    ptr = strstr(buffer, "a:");
                    if (ptr)
                    {
                        ptr += strlen("a:");
                        UVFound = 1;
                    }
                }

                if (UVFound)
                {
                    while (*ptr && isspace((unsigned char)*ptr)) ptr++;

                    if (!isdigit((unsigned char)*ptr) && *ptr != '-' && *ptr != '.') 
                        break;

                    while (sscanf(ptr, "%f", &UV) == 1)
                    {
                        UVCoord.push_back(UV);

                        while (*ptr != ',' && *ptr != '\0') ptr++;
                        if (*ptr == ',') ptr++;
                    }
                }

                position = ftell(file);
            }
        }
        else if (strstr(buffer, "UVIndex:"))
        {
            int UVIndexFound = 0;
            while (fgets(buffer, sizeof(buffer), file))
            {
                ptr = buffer;
                if (!UVIndexFound)
                {
                    ptr = strstr(buffer, "a:");
                    if (ptr)
                    {
                        ptr += strlen("a:");
                        UVIndexFound = 1;
                    }
                }

                if (UVIndexFound)
                {
                    while (*ptr && isspace((unsigned char)*ptr)) ptr++;

                    if (!isdigit((unsigned char)*ptr) && *ptr != '-' && *ptr != '.') 
                        break;

                    while (sscanf(ptr, "%d", &UVIndex) == 1)
                    {       
                        UVCoordIndex.push_back(UVIndex);

                        while (*ptr != ',' && *ptr != '\0') ptr++;
                        if (*ptr == ',') ptr++;
                    }
                }

                position = ftell(file);
            }
        }

        // UVセグメントの終わり
        else if (strstr(buffer, "}"))
        {
            int UVCoordIndexSize = UVCoordIndex.getSize();

            if (mappingInformationType == ByPolygonVertex
                && referenceInformationType == IndexToDirect)
            {

                for (int i = 0; i < UVCoordIndexSize; i++)
                {                    
                    int coordIndex = UVCoordIndex[i];

                    XMFLOAT3 normal0, normal1, normal2, pos0, pos1, pos2;
                    if ((i+1) % 3 == 0 && i > 1)
                    {
                        if (meshData->normalLoc == VertexDataLocation::Index)
                        {
                            normal0 = meshData->mesh->IndicesData[i].Normal;
                            normal1 = meshData->mesh->IndicesData[i - 1].Normal;
                            normal2 = meshData->mesh->IndicesData[i - 2].Normal;
                        }
                        else if (meshData->normalLoc == VertexDataLocation::Vertex)
                        {
                            //normal0 = modelData->mesh->VerticesTemp[i].Normal;
                            //normal1 = modelData->mesh->VerticesTemp[i - 1].Normal;
                            //normal2 = modelData->mesh->VerticesTemp[i - 2].Normal;
                        }
                        int vertexIndex0 = meshData->mesh->IndicesData[i].Index;
                        int vertexIndex1 = meshData->mesh->IndicesData[i - 1].Index;
                        int vertexIndex2 = meshData->mesh->IndicesData[i - 2].Index;
                        if (vertexIndex0 < 0)
                        {
                            vertexIndex0 = -vertexIndex0;
                            vertexIndex0--;
                        }
                        pos0 = meshData->mesh->VerticesTemp[vertexIndex0].Position;
                        pos1 = meshData->mesh->VerticesTemp[vertexIndex1].Position;
                        pos2 = meshData->mesh->VerticesTemp[vertexIndex2].Position;
                    }


                    if (geometryType == GeometryType::Mesh)
                    {
                        if (UVCoordIndexSize != meshData->mesh->IndicesData.getSize())
                            return false;
                        meshData->mesh->IndicesData[i].TexCoord.x = UVCoord[coordIndex * 2];
                        meshData->mesh->IndicesData[i].TexCoord.y = UVCoord[coordIndex * 2 + 1];

                       
                        if ((i + 1) % 3 == 0 && i > 1)
                            CalculateTangentAndBitangent(pos0, pos1, pos2,
                                meshData->mesh->IndicesData[i].TexCoord,
                                meshData->mesh->IndicesData[i - 1].TexCoord,
                                meshData->mesh->IndicesData[i - 2].TexCoord,
                                normal0, normal1, normal2,
                                meshData->mesh->IndicesData[i].Tangent,
                                meshData->mesh->IndicesData[i - 1].Tangent,
                                meshData->mesh->IndicesData[i - 2].Tangent,
                                meshData->mesh->IndicesData[i].Bitangent,
                                meshData->mesh->IndicesData[i - 1].Bitangent,
                                meshData->mesh->IndicesData[i - 2].Bitangent);


                    }
                    else if (geometryType == GeometryType::Shape)
                    {
                        if (modelMeshData == nullptr)
                            modelMeshData = new ModelMeshData();

                        modelMeshData->IndicesData[i].TexCoord.x = UVCoord[coordIndex * 2];
                        modelMeshData->IndicesData[i].TexCoord.y = UVCoord[coordIndex * 2 + 1];

                        if ((i + 1) % 3 == 0 && i > 1)
                            CalculateTangentAndBitangent(pos0, pos1, pos2,
                                modelMeshData->IndicesData[i].TexCoord,
                                modelMeshData->IndicesData[i - 1].TexCoord,
                                modelMeshData->IndicesData[i - 2].TexCoord,
                                normal0, normal1, normal2,
                                modelMeshData->IndicesData[i].Tangent,
                                modelMeshData->IndicesData[i - 1].Tangent,
                                modelMeshData->IndicesData[i - 2].Tangent,
                                modelMeshData->IndicesData[i].Bitangent,
                                modelMeshData->IndicesData[i - 1].Bitangent,
                                modelMeshData->IndicesData[i - 2].Bitangent);
                    }

                }
            }
            else if (mappingInformationType == ByVertice
                && referenceInformationType == Direct)
            {
                int numVertices = meshData->mesh->VerticesTemp.getSize();
                if (UVCoord.getSize() != numVertices * 2)
                    return false;

                for (int i = 0; i < numVertices; i++)
                {
                    meshData->mesh->VerticesTemp[i].TexCoord.x = UVCoord[i * 2];
                    meshData->mesh->VerticesTemp[i].TexCoord.y = UVCoord[i * 2 + 1];
                }
            }

            return true;
        }

        position = ftell(file);
    }

    return false;
}

bool FBXLoader::ParseTexture(FILE* file, SkinnedMeshModelData* modelData, FbxNode* node)
{
    char buffer[512];
    char* textureName = new char[TEXTURE_NAME_LENGTH];
    textureName[0] = '\0';
    while (fgets(buffer, sizeof(buffer), file))
    {
        if (strstr(buffer, "Properties70"))
            SkipNode(file);
        else if (strstr(buffer, "RelativeFilename"))
        {
            if (sscanf(buffer, " RelativeFilename: \"%[^\"]\"", textureName) != 1)
            {
				textureName = nullptr;
            }
                
        }
        else if (strstr(buffer, "}"))
        {
			if (textureName != nullptr)
			{
                node->nodeData = static_cast<void*>(textureName);
			}
            return true;
        }
    }
    delete textureName;
    return false;
}

bool FBXLoader::ParseObjectConnections(FILE* file, SkinnedMeshModelData* modelData)
{
    char buffer[512];
    uint64_t nodeID1, nodeID2;

    while (fgets(buffer, sizeof(buffer), file))
    {
        char attributeType[64];
        if (sscanf(buffer, " C: \"OO\",%llu,%llu", &nodeID1, &nodeID2) == 2)
        {
            if (nodeID2 == 0)
                nodeID2 = modelData->currentRootNodeID - 1;

            FbxNode** ppChildNode = modelData->fbxNodes.search(nodeID1);
            FbxNode** ppParentNode = modelData->fbxNodes.search(nodeID2);
            if (ppChildNode == nullptr || ppParentNode == nullptr) continue;

            (*ppChildNode)->parentNodes.push_back(*ppParentNode);
            (*ppParentNode)->childNodes.push_back(*ppChildNode);
        }
        else if (sscanf(buffer, " C: \"OP\",%llu,%llu, \"%[^\"]\"", &nodeID1, &nodeID2, &attributeType) == 3)
        {
            FbxNode** ppChildNode = modelData->fbxNodes.search(nodeID1);
            FbxNode** ppParentNode = modelData->fbxNodes.search(nodeID2);
            if (ppChildNode == nullptr || ppParentNode == nullptr) continue;

            if (strcmp(attributeType, "d|X") == 0)
            {
                AnimationCurveNode* animationCurveNode = static_cast<AnimationCurveNode*>((*ppParentNode)->nodeData);
                if (animationCurveNode == nullptr)
                    animationCurveNode = new AnimationCurveNode();
                animationCurveNode->dX = (*ppChildNode)->nodeID;
                (*ppParentNode)->nodeData = static_cast<void*>(animationCurveNode);
            }
            else if (strcmp(attributeType, "d|Y") == 0)
            {
                AnimationCurveNode* animationCurveNode = static_cast<AnimationCurveNode*>((*ppParentNode)->nodeData);
                if (animationCurveNode == nullptr)
                    animationCurveNode = new AnimationCurveNode();
                animationCurveNode->dY = (*ppChildNode)->nodeID;
                (*ppParentNode)->nodeData = static_cast<void*>(animationCurveNode);
            }
            else if (strcmp(attributeType, "d|Z") == 0)
            {
                AnimationCurveNode* animationCurveNode = static_cast<AnimationCurveNode*>((*ppParentNode)->nodeData);
                if (animationCurveNode == nullptr)
                    animationCurveNode = new AnimationCurveNode();
                animationCurveNode->dZ = (*ppChildNode)->nodeID;
                (*ppParentNode)->nodeData = static_cast<void*>(animationCurveNode);
            }
            else if (strcmp(attributeType, "Lcl Translation") == 0)
            {
                LimbNodeAnimation* limbNodeAnimation = static_cast<LimbNodeAnimation*>((*ppParentNode)->limbNodeAnimation);
                if (limbNodeAnimation == nullptr)
                    limbNodeAnimation = new LimbNodeAnimation();
                limbNodeAnimation->LclTranslation = (*ppChildNode)->nodeID;
                (*ppParentNode)->limbNodeAnimation = static_cast<void*>(limbNodeAnimation);
            }
            else if (strcmp(attributeType, "Lcl Rotation") == 0)
            {
                LimbNodeAnimation* limbNodeAnimation = static_cast<LimbNodeAnimation*>((*ppParentNode)->limbNodeAnimation);
                if (limbNodeAnimation == nullptr)
                    limbNodeAnimation = new LimbNodeAnimation();
                limbNodeAnimation->LclRot = (*ppChildNode)->nodeID;
                (*ppParentNode)->limbNodeAnimation = static_cast<void*>(limbNodeAnimation);
            }
            else if (strcmp(attributeType, "Lcl Scaling") == 0)
            {
                LimbNodeAnimation* limbNodeAnimation = static_cast<LimbNodeAnimation*>((*ppParentNode)->limbNodeAnimation);
                if (limbNodeAnimation == nullptr)
                    limbNodeAnimation = new LimbNodeAnimation();
                limbNodeAnimation->LclScl = (*ppChildNode)->nodeID;
                (*ppParentNode)->limbNodeAnimation = static_cast<void*>(limbNodeAnimation);
            }
            else if (strcmp(attributeType, "EmissiveColor") == 0)
            {
                char* textureName = static_cast<char*>((*ppChildNode)->nodeData);
                if (textureName == nullptr)
                    return false;
                FbxMaterial* material = static_cast<FbxMaterial*>((*ppParentNode)->nodeData);
                strcpy(material->EmissiveColorTexName, textureName);
            }
            else if (strcmp(attributeType, "DiffuseColor") == 0)
            {
                char* textureName = static_cast<char*>((*ppChildNode)->nodeData);
                if (textureName == nullptr)
                    return false;
                FbxMaterial* material = static_cast<FbxMaterial*>((*ppParentNode)->nodeData);
                strcpy(material->DiffuseColorTexName, textureName);
            }
            (*ppChildNode)->parentNodes.push_back(*ppParentNode);
            (*ppParentNode)->childNodes.push_back(*ppChildNode);
        }
        else if (strstr(buffer, "}"))
            return true;
    }
    return true;
}

bool FBXLoader::ParseBindPose(FILE* file, SkinnedMeshModelData* modelData, FbxNode* node)
{
    char buffer[4096];
    char* ptr;
    uint64_t modelNodeID;
    XMFLOAT4X4 mtxBindPose{};
    int count = 0;
    while (fgets(buffer, sizeof(buffer), file))
    {
        if (strstr(buffer, "NbPoseNodes:"))
        {
            if (sscanf(buffer, " NbPoseNodes: %d", &modelData->bindPose.numePoseNodes) != 1)
            {
                return false;
            }
        }
        else if (strstr(buffer, "PoseNode:"))
        {
            while (fgets(buffer, sizeof(buffer), file))
            {

                if (strstr(buffer, "Node:"))
                {
                    if (sscanf(buffer, " Node: %llu", &modelNodeID) != 1)
                    {
                        return false;
                    }
                }

                else if (strstr(buffer, "Matrix:"))
                {
                    int matrixFound = 0;
                    SimpleArray<float> matrixArray;
                    float mtxEntry;
                    while (fgets(buffer, sizeof(buffer), file))
                    {
                        ptr = buffer;
                        if (!matrixFound)
                        {
                            ptr = strstr(buffer, "a:");
                            if (ptr)
                            {
                                ptr += strlen("a:");
                                matrixFound = 1;
                            }
                        }

                        if (matrixFound)
                        {
                            while (*ptr && isspace((unsigned char)*ptr)) ptr++;

                            if (!isdigit((unsigned char)*ptr) && *ptr != '-' && *ptr != '.')
                                break;

                            while (sscanf(ptr, "%f", &mtxEntry) == 1)
                            {
                                matrixArray.push_back(mtxEntry);

                                while (*ptr != ',' && *ptr != '\0') ptr++;
                                if (*ptr == ',') ptr++;
                            }
                        }
                    }
                    if (matrixArray.getSize() != 16)
                    {
                        return false;
                    }

                    int index = 0;
                    for (int col = 0; col < 4; col++)
                    {
                        for (int row = 0; row < 4; row++)
                        {
                            mtxBindPose.m[col][row] = matrixArray[index++];
                        }
                    }
                }
                else if (strstr(buffer, "}"))
                {
                    modelData->bindPose.mtxBindPoses.insert(modelNodeID, mtxBindPose);
                    count++;
                    break;
                }
                    
            }
        }
        else if (strstr(buffer, "}"))
        {
            if (count != modelData->bindPose.numePoseNodes)
            {
                modelData->bindPose.numePoseNodes = 0;
                modelData->bindPose.mtxBindPoses.clear();
                return false;
            }
            else
            {
                return true;
            }

        }
    }
    return false;
}

bool FBXLoader::ParseAnimationStackCurve(FILE* file, SkinnedMeshModelData* modelData)
{
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), file))
    {
        if (strstr(buffer, "LocalStop"))
        {
            if (modelData->currentAnimClip && sscanf(buffer, "%*[^0-9]%llu", &modelData->currentAnimClip->stopTime) != 1)
            {
                return false;
            }
        }
        if (strstr(buffer, "}"))
        {
            fgets(buffer, sizeof(buffer), file);
            if (strstr(buffer, "}"))
                return true;
        }
    }
    return false;
}

bool FBXLoader::ParseAnimationCurve(FILE* file, FbxNode* node)
{
    char buffer[4096];
    char* ptr;
    AnimationCurve* animationCurve = new AnimationCurve();
    while (fgets(buffer, sizeof(buffer), file))
    {
        if (strstr(buffer, "Default:"))
        {
            if (sscanf(buffer, " Default: %f", &animationCurve->DefaultValue) != 1)
            {
                delete animationCurve; 
                return false;
            }
        }
        else if (strstr(buffer, "KeyTime:"))
        {
            int keyTimeFound = 0;
            uint64_t keyTime;
            while (fgets(buffer, sizeof(buffer), file))
            {
                ptr = buffer;
                if (!keyTimeFound)
                {
                    ptr = strstr(buffer, "a:");
                    if (ptr)
                    {
                        ptr += strlen("a:");
                        keyTimeFound = 1;
                    }
                }

                if (keyTimeFound)
                {
                    while (*ptr && isspace((unsigned char)*ptr)) ptr++;

                    if (!isdigit((unsigned char)*ptr) && *ptr != '-' && *ptr != '.')
                        break;

                    while (sscanf(ptr, "%llu", &keyTime) == 1)
                    {
                        animationCurve->KeyTime.push_back(keyTime);

                        while (*ptr != ',' && *ptr != '\0') ptr++;
                        if (*ptr == ',') ptr++;
                    }
                }
            }
        }
        else if (strstr(buffer, "KeyValueFloat:"))
        {
            int keyValueFound = 0;
            float keyValue;
            while (fgets(buffer, sizeof(buffer), file))
            {
                ptr = buffer;
                if (!keyValueFound)
                {
                    ptr = strstr(buffer, "a:");
                    if (ptr)
                    {
                        ptr += strlen("a:");
                        keyValueFound = 1;
                    }
                }

                if (keyValueFound)
                {
                    while (*ptr && isspace((unsigned char)*ptr)) ptr++;

                    if (!isdigit((unsigned char)*ptr) && *ptr != '-' && *ptr != '.')
                        break;

                    while (sscanf(ptr, "%f", &keyValue) == 1)
                    {
                        animationCurve->KeyValue.push_back(keyValue);

                        while (*ptr != ',' && *ptr != '\0') ptr++;
                        if (*ptr == ',') ptr++;
                    }
                }
            }
        }
        else if (strstr(buffer, "KeyAttrFlags"))
        {
            if(strstr(buffer, "{"))
                SkipNode(file);
        }
        else if (strstr(buffer, "KeyAttrDataFloat"))
        {
            if (strstr(buffer, "{"))
                SkipNode(file);
        }
        else if (strstr(buffer, "KeyAttrRefCount:"))
        {
            SkipNode(file);
        }
        else if (strstr(buffer, "}"))
        {
            if (animationCurve->KeyTime.getSize() != animationCurve->KeyValue.getSize())
            {
                delete animationCurve;
                return false;
            }
            else
            {
                animationCurve->KeyAttrRefCount = animationCurve->KeyTime.getSize();
                node->nodeData = static_cast<void*>(animationCurve);
                return true;
            }

        }

    }
    return false;
}

void FBXLoader::SkipNode(FILE* file)
{
    char buffer[4096];
    int bracket = 1;
    long int pos = 0;
    while (fgets(buffer, sizeof(buffer), file))
    {
        if (strstr(buffer, "{"))
            bracket++;
        else if (strstr(buffer, "}"))
            bracket--;
        if (bracket == 0)
            return;

        pos = ftell(file);
    }
}

void FBXLoader::HandleDeformer(FbxNode* node, MeshData* meshData, SkinnedMeshModelData* modelData, int curIdx, int prevIdx)
{
    
    Deformer* deformer = static_cast<Deformer*>(node->nodeData);
    if (deformer)
    {
        deformer->boneIdx = curIdx;
        meshData->mBoneHierarchy.push_back(prevIdx);
        meshData->mBoneOffsets.push_back(deformer->TransformLink);
        meshData->mBoneToParentTransforms.push_back(deformer->Transform);

        //modelData->deformerHashMap.insert(curIdx, node->nodeID);
        //modelData->deformerIdxHashMap.insert(node->nodeID, curIdx);

        modelData->deformerHashMap.insert(curIdx, node->nodeID);
        modelData->deformerIdxHashMap.insert(node->nodeID, curIdx);

        for (UINT i = 0; i < deformer->Index.getSize(); i++)
        {
            int vertexIdx = deformer->Index[i];
            float weight = deformer->Weights[i];
            meshData->mesh->VerticesTemp[vertexIdx].Weights.push_back(weight);
            meshData->mesh->VerticesTemp[vertexIdx].BoneIndices.push_back(curIdx);

        }

        meshData->IndexNum += deformer->Index.getSize();

        for (UINT i = 0; i < node->childNodes.getSize(); i++)
        {
            if (node->childNodes[i]->nodeType == FbxNodeType::Cluster)
            {
                int prev = deformer->boneIdx;
                curIdx++;
                HandleDeformer(node->childNodes[i], meshData, modelData, curIdx, prev);
            }
            else if (node->childNodes[i]->nodeType == FbxNodeType::LimbNode)
            {
                modelData->deformerToLimb.insert(node->nodeID, node->childNodes[i]->nodeID);
            }

        }
    }
}

void FBXLoader::HandleMeshNode(FbxNode* node, SkinnedMeshModelData* modelData)
{
    SimpleArray<FbxNode*> meshNodeChilds = node->childNodes;
    FbxNode* geometryNode = nullptr;
    FbxNode* materialNode = nullptr;

    for (UINT i = 0; i < meshNodeChilds.getSize(); i++)
    {
        if (meshNodeChilds[i]->nodeType == FbxNodeType::Mesh)
        {

            geometryNode = meshNodeChilds[i];
        }
        else if (meshNodeChilds[i]->nodeType == FbxNodeType::Material)
        {
            materialNode = meshNodeChilds[i];
        }
    }

    if (geometryNode)
    {
        MeshData** ppMeshData = modelData->meshDataMap.search(geometryNode->nodeID);
        if (ppMeshData)
        {
            SimpleArray<FbxNode*> geometryNodeChilds = geometryNode->childNodes;
            if (materialNode)
                (*ppMeshData)->material = static_cast<FbxMaterial*>(materialNode->nodeData);
            
            for (UINT k = 0; k < geometryNodeChilds.getSize(); k++)
            {
                FbxNode* deformerArmature = geometryNodeChilds[k];
                FbxNode* modelArmature = GetModelArmatureNodeByDeformer(deformerArmature);
                if (modelArmature)
                {
                    (*ppMeshData)->armatureNode = modelArmature;
                    if (modelData->currentAnimClip)
                    {
                        modelData->currentAnimClip->armatureNode = modelArmature;
                        if (!modelData->animationClips.search(modelData->currentAnimClip->name))
                            modelData->animationClips.insert(modelData->currentAnimClip->name, *modelData->currentAnimClip);
                    }
                }
                    
                HandleDeformer(deformerArmature, *ppMeshData, modelData, 0, -1);
            }
        }
    }

}

void FBXLoader::BuildLimbHierarchy(FbxNode* armatureNode, MeshData* modelData, int curIdx, int prevIdx)
{
    SimpleArray<FbxNode*> childNodes = armatureNode->childNodes;
    for (UINT i = 0; i < childNodes.getSize(); i++)
    {
        if (childNodes[i]->nodeType == FbxNodeType::LimbNode)
        {
            modelData->mBoneHierarchy.push_back(prevIdx);

        }
        
    }
}

FbxNode* FBXLoader::GetGeometryNodeByLimbNode(FbxNode* limbNode)
{
    SimpleArray<FbxNode*> pParentNodes = limbNode->parentNodes;
    for (UINT i = 0; i < pParentNodes.getSize(); i++)
    {
        if (pParentNodes[i]->nodeType == FbxNodeType::Cluster
            || pParentNodes[i]->nodeType == FbxNodeType::Skin)
        {
            return GetGeometryNodeByLimbNode(pParentNodes[i]);
        }
        else if (pParentNodes[i]->nodeType == FbxNodeType::Mesh)
        {
            return pParentNodes[i];
        }
    }
    return nullptr;
}

FbxNode* FBXLoader::GetModelArmatureNodeByDeformer(FbxNode* deformerNode)
{
    SimpleArray<FbxNode*> subDeformerNodes = deformerNode->childNodes;
    for (UINT i = 0; i < subDeformerNodes.getSize(); i++)
    {
        SimpleArray<FbxNode*> modelNodes = subDeformerNodes[i]->childNodes;
        for (UINT j = 0; j < modelNodes.getSize(); j++)
        {
            FbxNode* armatureNode = GetModelArmatureNodeByModel(modelNodes[j]);
            if (armatureNode)
                return armatureNode;
        }
    }
    return nullptr;
}

FbxNode* FBXLoader::GetModelArmatureNodeByModel(FbxNode* modelNode)
{
    SimpleArray<FbxNode*> parentModelNodes = modelNode->parentNodes;
    for (UINT i = 0; i < parentModelNodes.getSize(); i++)
    {
        if (parentModelNodes[i]->nodeType == FbxNodeType::LimbNode)
        {
            if (parentModelNodes[i]->parentNodes[0]->nodeID == 0)
                return parentModelNodes[i];
            else
                return GetModelArmatureNodeByModel(parentModelNodes[i]);
        }
        else if (parentModelNodes[i]->nodeType == FbxNodeType::None)
            return parentModelNodes[i];
    }
    return nullptr;
}

char* FBXLoader::GetTextureName(char* modelPath, char* relativeTexturePath)
{
    if (modelPath == nullptr || relativeTexturePath == nullptr)
        return nullptr;
    char* result = new char[TEXTURE_NAME_LENGTH];
    result[0] = '\0';

    const char* lastSlash = strrchr(relativeTexturePath, '/');
    const char* lastBackslash = strrchr(relativeTexturePath, '\\');

    // 最後の区切り文字がどちらかを確認し、それに基づいてファイル名を取得
    const char* textureNameStart = relativeTexturePath;
    if (lastSlash || lastBackslash) 
    {
        textureNameStart = max(lastSlash, lastBackslash) + 1;
    }

    strcpy(result, modelPath);

    size_t modelPathLength = strlen(modelPath);
    if (modelPathLength > 0 && modelPath[modelPathLength - 1] != '/' && modelPath[modelPathLength - 1] != '\\')
    {
        strcat(result, "/");
    }
    // ファイル名部分を追加
    strcat(result, textureNameStart);

    return result;
}

void FBXLoader::CalculateTangentAndBitangent(
    const XMFLOAT3& p0, const XMFLOAT3& p1, const XMFLOAT3& p2,
    const XMFLOAT2& TexCoord0, const XMFLOAT2& TexCoord1, const XMFLOAT2& TexCoord2,
    const XMFLOAT3& n0, const XMFLOAT3& n1, const XMFLOAT3& n2,
    XMFLOAT3& tangent0, XMFLOAT3& tangent1, XMFLOAT3& tangent2,
    XMFLOAT3& bitangent0, XMFLOAT3& bitangent1, XMFLOAT3& bitangent2)
{
    XMVECTOR pos1 = XMLoadFloat3(&p0);
    XMVECTOR pos2 = XMLoadFloat3(&p1);
    XMVECTOR pos3 = XMLoadFloat3(&p2);

    XMVECTOR uv1 = XMLoadFloat2(&TexCoord0);
    XMVECTOR uv2 = XMLoadFloat2(&TexCoord1);
    XMVECTOR uv3 = XMLoadFloat2(&TexCoord2);

    XMVECTOR edge1 = pos2 - pos1;
    XMVECTOR edge2 = pos3 - pos1;
    XMVECTOR deltaUV1 = uv2 - uv1;
    XMVECTOR deltaUV2 = uv3 - uv1;


    float r = 1.0f / (XMVectorGetX(deltaUV1) * XMVectorGetY(deltaUV2) - XMVectorGetX(deltaUV2) * XMVectorGetY(deltaUV1));
    XMVECTOR tangentVec = (edge1 * XMVectorGetY(deltaUV2) - edge2 * XMVectorGetY(deltaUV1)) * r;

    XMVECTOR bitangentVec0 = XMVector3Cross(XMLoadFloat3(&n0), tangentVec);
    XMVECTOR bitangentVec1 = XMVector3Cross(XMLoadFloat3(&n1), tangentVec);
    XMVECTOR bitangentVec2 = XMVector3Cross(XMLoadFloat3(&n2), tangentVec);

    XMStoreFloat3(&tangent0, XMVector3Normalize(tangentVec));
    XMStoreFloat3(&tangent1, XMVector3Normalize(tangentVec));
    XMStoreFloat3(&tangent2, XMVector3Normalize(tangentVec));

    XMStoreFloat3(&bitangent0, XMVector3Normalize(bitangentVec0));
    XMStoreFloat3(&bitangent1, XMVector3Normalize(bitangentVec1));
    XMStoreFloat3(&bitangent2, XMVector3Normalize(bitangentVec2));
}


bool FBXLoader::CreateFbxNode(char* buffer, FbxNode* fbxNode)
{
    char nodeNameBuffer[128];
    char nodeTypeBuffer[128];
    char nodeBuffer[128];
    FbxNodeType nodeType = FbxNodeType::None;
    uint64_t nodeID;
    if (sscanf(buffer, " %[^:]: %lld, \"%[^\"]\", \"%[^\"]\"", nodeBuffer, &nodeID, nodeNameBuffer, nodeTypeBuffer) < 2)
        return false;
    if (strcmp(nodeTypeBuffer, "LimbNode") == 0)
        nodeType = FbxNodeType::LimbNode;
    else if (strcmp(nodeTypeBuffer, "Mesh") == 0)
        nodeType = FbxNodeType::Mesh;
    else if (strcmp(nodeTypeBuffer, "Skin") == 0)
        nodeType = FbxNodeType::Skin;
    else if (strcmp(nodeTypeBuffer, "BlendShape") == 0)
        nodeType = FbxNodeType::BlendShape;
    else if (strcmp(nodeTypeBuffer, "BlendShapeChannel") == 0)
        nodeType = FbxNodeType::BlendShapeChannel;
    else if (strcmp(nodeTypeBuffer, "Cluster") == 0)
        nodeType = FbxNodeType::Cluster;
    else if (strcmp(nodeTypeBuffer, "BindPose") == 0)
        nodeType = FbxNodeType::BindPose;
    else if (strcmp(nodeBuffer, "AnimationCurve") == 0)
        nodeType = FbxNodeType::AnimationCurve;
    else if (strcmp(nodeBuffer, "AnimationCurveNode") == 0)
        nodeType = FbxNodeType::AnimationCurveNode;

    fbxNode->nodeID = nodeID;
    strcpy(fbxNode->nodeName, nodeNameBuffer);
    fbxNode->nodeType = nodeType;

    return true;
}

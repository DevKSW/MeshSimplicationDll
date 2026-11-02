#include "pch.h"
#include "ModelParser.h"
#include "MeshStoreBox.h"
#include "Logger.h"
#include <set>
#include <array>
#include <memory>
#include <assimp/SceneCombiner.h>


MeshStoreBox* ModelParser::LoadModel(const std::string& path)
{	       

    MeshStoreBox* meshStoreBox = new MeshStoreBox();

    try
    {
    const aiScene* scene = this->importer.ReadFile(path,
        aiProcess_Triangulate |           // 모든 면을 삼각형으로 변환
        //aiProcess_FlipUVs |               // DirectX/OpenGL 좌표계 차이 해결
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenNormals              // Generate only when source normals are absent.
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string errorStr = importer.GetErrorString();
        throw std::exception(errorStr.c_str()); // 모델 실패 , 메쉬 미존재 시 Throw
    }

    LOG_INFO("Load Model : \n  \t\t\t mesh count {}", scene->mNumMeshes);
    // 메시 순회


    unsigned int arrAdjustNum = 0;
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) 
    {
        aiMesh* mesh = scene->mMeshes[i];
        LOG_INFO("Load Model : \n  \t\t\t vertex count {} , face count {} ", mesh->mNumVertices, mesh->mNumFaces);
        // Exctract vertex data
        for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
            aiVector3D pos = mesh->mVertices[j];            
            // 여기서 정점을 구조체에 저장            
            QEMVertex& v = meshStoreBox->AddVertex(QEMVertex(pos.x, pos.y, pos.z));                        
                        
        }

        //Extract Bone Data
        if (mesh->HasBones()) 
        {
            for (size_t i = 0; i < mesh->mNumBones; i++)
            {
                
            }

        }
        
        // Exctract face(index) data
        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            aiFace face = mesh->mFaces[j];            

            if (face.mNumIndices != 3) continue;

            QEMFace qemFace;
            qemFace.indices[0] = meshStoreBox->GetVertexAt(arrAdjustNum + face.mIndices[0]);
            qemFace.indices[1] = meshStoreBox->GetVertexAt(arrAdjustNum + face.mIndices[1]);
            qemFace.indices[2] = meshStoreBox->GetVertexAt(arrAdjustNum + face.mIndices[2]);            
            qemFace.metaData.subMeshID = i;
            qemFace.metaData.materialIndex = mesh->mMaterialIndex;            

            // UV 정보 저장  
            for (int corner = 0; corner < 3; ++corner) {
                unsigned int origVertIdx = face.mIndices[corner];
                if (mesh->HasNormals()) {
                    const auto& normal = mesh->mNormals[origVertIdx];
                    qemFace.normals[corner] = DirectX::XMFLOAT3(normal.x, normal.y, normal.z);
                }

                for (int ch = 0; ch < 8; ++ch) {
                    aiVector3D* texCoord = mesh->mTextureCoords[ch];
                    if (!texCoord) continue;

                    UVElement element(ch, texCoord[origVertIdx].x, texCoord[origVertIdx].y);
                    qemFace.uvs[corner][ch] = element;
                }
            }



            meshStoreBox->AddFace(qemFace);
        }
        arrAdjustNum += mesh->mNumVertices;       

    }    

    
    // SetMaterials 
    for (unsigned int i = 0; i < scene->mNumMaterials; i++)
    {
        aiMaterial* material = scene->mMaterials[i];        
        meshStoreBox->AddMaterial(material);
    }
    // Own embedded image data independently of the importer's next ReadFile().
    for (unsigned int i = 0; i < scene->mNumTextures; ++i)
        meshStoreBox->AddTexture(scene->mTextures[i]);
        

    // Unpack Sekeleton 
    //if (scene->HasSkeletons()) 
    //{
    //    for (unsigned int i = 0; i < scene->mNumSkeletons; i++)
    //    {
    //        auto skeleton = scene->mSkeletons[i];           
    //        for (unsigned int j = 0; j < skeleton->mNumBones; j++)
    //        {
    //            auto bone = skeleton->mBones[j];                
    //            auto meshID = bone->mMeshId;                                
    //            for (auto weightID = 0; weightID < bone->mNumnWeights; weightID++)
    //            {                    
    //                auto vID = bone->mWeights[i].mVertexId;
    //                auto weight = bone->mWeights[i].mWeight;
    //                meshID->mVertices[vID];
    //                
    //            }
    //
    //        }
    //
    //    } 
    //}

    if (scene->HasAnimations()) 
    {
        for (int i = 0; i < scene->mNumAnimations; i++)
        {
            meshStoreBox->AddAnimation(scene->mAnimations[i]);
        }
    }
        
        
        LOG_INFO("Load Model MeshBox : \n\t\t\t  vertex count {} , face count {}", meshStoreBox->GetVertexCount() , meshStoreBox->GetFaceCount() );

    }
        

    catch (const std::exception& e)
    {   
        LOG_ERROR("ERROR : {} in LoadModel", e.what());        
        delete meshStoreBox;
        meshStoreBox = nullptr;        
    }

    return meshStoreBox;
}

bool ModelParser::ExportModel(MeshStoreBox* storeBox, const std::string& path, CoordinateFlag coordinate, const std::string& filePath)
{
    if (!storeBox) return false;

    // ─────────────────────────────────────────────
    // 1. Group live faces by submesh and material assignment.
    // ─────────────────────────────────────────────         

    std::map<std::pair<int, int>, std::vector<QEMFace*>> bySubMesh;
    for (auto* f : storeBox->GetFaces()) {
        if (!f || f->isDeleted) continue;
        if (!f->indices[0] || !f->indices[1] || !f->indices[2]) continue;
        if (f->indices[0] == f->indices[1] ||
            f->indices[1] == f->indices[2] ||
            f->indices[0] == f->indices[2]) continue;

        bySubMesh[{f->metaData.subMeshID, f->metaData.materialIndex}].push_back(f);
    }

    if (bySubMesh.empty()) {
        LOG_ERROR("ExportModel: no live faces to export");
        return false;
    }

    // ─────────────────────────────────────────────
    // 2. aiScene / 머티리얼 / 루트 노드 골격
    // ─────────────────────────────────────────────
    auto scene = std::make_unique<aiScene>();
    scene->mRootNode = new aiNode();

    const unsigned int meshCount = static_cast<unsigned int>(bySubMesh.size());

    scene->mNumMeshes = meshCount;
    scene->mMeshes = new aiMesh * [meshCount]{};

    const auto& materials = storeBox->GetMaterials();
    scene->mNumMaterials = max(1u, static_cast<unsigned int>(materials.size()));
    scene->mMaterials = new aiMaterial * [scene->mNumMaterials]{};
    for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
        scene->mMaterials[i] = new aiMaterial();
        if (i < materials.size()) {
            aiMaterial::CopyPropertyList(scene->mMaterials[i], materials[i]);
        } else {
            aiString matName("Material_0");
            scene->mMaterials[i]->AddProperty(&matName, AI_MATKEY_NAME);
        }
    }

    const auto& textures = storeBox->GetTextures();
    scene->mNumTextures = static_cast<unsigned int>(textures.size());
    if (!textures.empty()) {
        scene->mTextures = new aiTexture * [textures.size()]{};
        for (size_t i = 0; i < textures.size(); ++i)
            Assimp::SceneCombiner::Copy(&scene->mTextures[i], textures[i]);
    }

    scene->mRootNode->mNumMeshes = meshCount;
    scene->mRootNode->mMeshes = new unsigned int[meshCount];

    // ─────────────────────────────────────────────
    // 3. 서브메시별 aiMesh 빌드
    // ─────────────────────────────────────────────

    // Split vertices at UV seams and normal discontinuities after position welding.
    struct UVKey
    {
        // 채널별 UV를 int32로 quantize하여 해시에 사용
        // (float을 그대로 비교하면 QEM 보간 후 미세한 차이로 seam이
        //  과도하게 split되므로, 허용 오차 epsilon 안에서는 같은 키로 묶는다)
        //static const float UV_QUANTIZE = 1e-4f; // seam 판정 허용 오차

        QEMVertex* ptr;
        std::array<float, 3> normal;
        std::vector<std::pair<int, std::pair<int32_t, int32_t>>> quantizedUVs; // channel → (qu, qv)

        bool operator==(const UVKey& o) const
        {
            if (ptr != o.ptr) return false;
            if (normal != o.normal) return false;
            if (quantizedUVs.size() != o.quantizedUVs.size()) return false;
            for (size_t i = 0; i < quantizedUVs.size(); ++i)
                if (quantizedUVs[i] != o.quantizedUVs[i]) return false;
            return true;
        }
    };

    struct UVKeyHash
    {
        size_t operator()(const UVKey& k) const
        {
            size_t h = std::hash<void*>()(k.ptr);
            for (float component : k.normal)
                h ^= std::hash<float>()(component) + 0x9e3779b9u + (h << 6) + (h >> 2);
            for (const auto& [ch, quv] : k.quantizedUVs)
            {
                h ^= std::hash<int>()(ch) * 2654435761u;
                h ^= std::hash<int32_t>()(quv.first) * 40503u;
                h ^= std::hash<int32_t>()(quv.second) * 12345u;
            }
            return h;
        }
    };

    // Build the split key from the corner's UVs and normalized normal.
    auto makeUVKey = [](QEMVertex* v, const std::map<int, UVElement>& uvMap,
        const aiVector3D& normal) -> UVKey
        {
            UVKey key;
            key.ptr = v;
            key.normal = { normal.x, normal.y, normal.z };
            for (const auto& [ch, elem] : uvMap)
            {
                int32_t qu = static_cast<int32_t>(std::floor(elem.u / 1e-3f/*UVKey::UV_QUANTIZE*/));
                int32_t qv = static_cast<int32_t>(std::floor(elem.v / 1e-3f/*UVKey::UV_QUANTIZE*/));
                key.quantizedUVs.push_back({ ch, {qu, qv} });
            }
            // 채널 번호 오름차순 정렬 → 삽입 순서와 무관하게 동일 키 보장
            std::sort(key.quantizedUVs.begin(), key.quantizedUVs.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });
            return key;
        };

    unsigned int meshSlot = 0;
    for (auto& kv : bySubMesh)
    {
        const int subID = kv.first.first;
        std::vector<QEMFace*>& groupFaces = kv.second;
        aiMesh* m = new aiMesh();
        scene->mMeshes[meshSlot] = m;
        m->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;

        int actualMatIdx = groupFaces[0]->metaData.materialIndex;
        if (actualMatIdx < 0 || actualMatIdx >= (int)scene->mNumMaterials)
            m->mMaterialIndex = 0;
        else
            m->mMaterialIndex = (unsigned int)actualMatIdx;

        std::unordered_map<UVKey, unsigned int, UVKeyHash> splitMap; // (ptr, UV, normal) -> localIdx
        std::vector<QEMVertex*>                             localVerts;
        std::vector<std::map<int, UVElement>>               localUvs;
        std::vector<aiVector3D> localNormals;

        // face index 배열 (face별 코너 3개의 로컬 인덱스를 미리 저장해 face 루프에서 재사용)
        std::vector<std::array<unsigned int, 3>> faceLocalIndices;
        faceLocalIndices.reserve(groupFaces.size());

        for (auto* f : groupFaces)
        {
            std::array<unsigned int, 3> corners{};
            for (int k = 0; k < 3; ++k)
            {
                QEMVertex* v = f->indices[k];
                const auto& n = f->normals[k];
                aiVector3D normal(n.x, n.y, n.z);
                if (!std::isfinite(normal.SquareLength()) || normal.SquareLength() < 1e-20f) {
                    const auto& a = f->indices[0]->position;
                    const auto& b = f->indices[1]->position;
                    const auto& c = f->indices[2]->position;
                    normal = aiVector3D(b.x - a.x, b.y - a.y, b.z - a.z) ^
                        aiVector3D(c.x - a.x, c.y - a.y, c.z - a.z);
                }
                normal.NormalizeSafe();
                UVKey ukey = makeUVKey(v, f->uvs[k], normal);

                auto it = splitMap.find(ukey);
                if (it == splitMap.end())
                {
                    // 새 조합 → 새 로컬 슬롯 할당
                    unsigned int newIdx = static_cast<unsigned int>(localVerts.size());
                    splitMap[ukey] = newIdx;
                    localVerts.push_back(v);
                    localUvs.push_back(f->uvs[k]); // 이 코너의 UV를 그대로 저장
                    localNormals.push_back(normal);
                    corners[k] = newIdx;
                }
                else
                {
                    corners[k] = it->second;
                }
            }
            faceLocalIndices.push_back(corners);
        }

        // split 통계 로그
        {
            size_t uniquePtrs = [&]() {
                std::unordered_set<QEMVertex*> s;
                for (auto* v : localVerts) s.insert(v);
                return s.size();
                }();
            LOG_INFO("Export submesh[{}] (subID={}): uniquePtrs={} -> splitVerts={}, F={}",
                meshSlot, subID, uniquePtrs, localVerts.size(), groupFaces.size());
        }

        // ── aiMesh 채우기 ──

        m->mNumVertices = static_cast<unsigned int>(localVerts.size());
        m->mVertices = new aiVector3D[m->mNumVertices];
        m->mNormals = new aiVector3D[m->mNumVertices];

        // 사용 UV 채널 검출
        std::set<int> usedChannels;
        for (const auto& uvMap : localUvs)
            for (const auto& [ch, _] : uvMap)
                usedChannels.insert(ch);

        int maxCh = usedChannels.empty() ? -1 : *usedChannels.rbegin();

        // 머티리얼이 텍스처를 가진 submesh는 최소 채널 0이 필요
        {
            aiMaterial* mat = scene->mMaterials[m->mMaterialIndex];
            aiString dummy;
            if (mat && mat->GetTexture(aiTextureType_DIFFUSE, 0, &dummy) == AI_SUCCESS)
                maxCh = (maxCh > 0 ? maxCh : 0);
        }

        // 0번부터 maxCh까지 연속 할당
        for (int ch = 0; ch <= maxCh; ++ch)
        {
            if (ch >= AI_MAX_NUMBER_OF_TEXTURECOORDS) break;
            m->mTextureCoords[ch] = new aiVector3D[m->mNumVertices];
            m->mNumUVComponents[ch] = 2;
            for (size_t i = 0; i < localVerts.size(); ++i)
            {
                auto it = localUvs[i].find(ch);
                if (it != localUvs[i].end())
                    m->mTextureCoords[ch][i] = aiVector3D(it->second.u, it->second.v, 0.0f);
                else
                    m->mTextureCoords[ch][i] = aiVector3D(0.0f, 0.0f, 0.0f);
            }
        }

        // 정점 위치 / 노말
        for (size_t i = 0; i < localVerts.size(); ++i)
        {
            const float x = localVerts[i]->position.x;
            const float y = localVerts[i]->position.y;
            const float z = localVerts[i]->position.z;
            const auto& normal = localNormals[i];

            switch (coordinate)
            {
            case CoordinateFlag::Unity:
                m->mVertices[i] = aiVector3D(x, y, -z);
                m->mNormals[i] = aiVector3D(normal.x, normal.y, -normal.z);
                break;
            case CoordinateFlag::Unreal:
                m->mVertices[i] = aiVector3D(-z, x, y);
                m->mNormals[i] = aiVector3D(-normal.z, normal.x, normal.y);
                break;
            case CoordinateFlag::Source:
            default:
                m->mVertices[i] = aiVector3D(x, y, z);
                m->mNormals[i] = normal;
                break;
            }
        }

        // 면 데이터 — faceLocalIndices에 미리 계산된 split 인덱스 사용
        m->mNumFaces = static_cast<unsigned int>(groupFaces.size());
        m->mFaces = new aiFace[m->mNumFaces];

        const bool flipWinding = (coordinate == CoordinateFlag::Unity ||
            coordinate == CoordinateFlag::Unreal);

        for (size_t i = 0; i < groupFaces.size(); ++i)
        {
            aiFace& af = m->mFaces[i];
            af.mNumIndices = 3;
            af.mIndices = new unsigned int[3];

            if (flipWinding)
            {
                af.mIndices[0] = faceLocalIndices[i][0];
                af.mIndices[1] = faceLocalIndices[i][2]; // swap
                af.mIndices[2] = faceLocalIndices[i][1]; // swap
            }
            else
            {
                af.mIndices[0] = faceLocalIndices[i][0];
                af.mIndices[1] = faceLocalIndices[i][1];
                af.mIndices[2] = faceLocalIndices[i][2];
            }
        }

        std::string meshName = "SubMesh_" + std::to_string(subID);
        m->mName.Set(meshName.c_str());

        scene->mRootNode->mMeshes[meshSlot] = meshSlot;
        ++meshSlot;
    }

    scene->mRootNode->mName.Set("Root");

    // Animations 
    const auto& sourceAnimations = storeBox->GetAnimations();
    const size_t count = sourceAnimations.size();       

    aiAnimation** animations = new aiAnimation * [count];

    for (size_t i = 0; i < count; ++i)
    {
        animations[i] = sourceAnimations[i];
    }

    scene->mNumAnimations = count;  
    scene->mAnimations = animations;

    // ─────────────────────────────────────────────
    // 4. FBX 저장
    // ─────────────────────────────────────────────
    try
    {
        LOG_INFO("export path : {}{}", filePath, path);
        Assimp::Exporter exporter;
        aiReturn res = exporter.Export(scene.get(), "fbx", filePath + path);
        if (res != aiReturn_SUCCESS)
            LOG_ERROR("Export failed: {}", exporter.GetErrorString());
        return (res == aiReturn_SUCCESS);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Error in Export : {}", e.what());
        return false;
    }
}

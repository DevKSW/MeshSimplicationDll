#include "pch.h"
#include "ModelParser.h"
#include "MeshStoreBox.h"
#include "Logger.h"
#include <set>


MeshStoreBox* ModelParser::LoadModel(const std::string& path)
{	       

    MeshStoreBox* meshStoreBox = new MeshStoreBox();

    try
    {
    const aiScene* scene = this->importer.ReadFile(path,
        aiProcess_Triangulate |           // 모든 면을 삼각형으로 변환
        //aiProcess_FlipUVs |               // DirectX/OpenGL 좌표계 차이 해결
        aiProcess_JoinIdenticalVertices //| // 중복 정점 제거 (QEM 준비 단계에서 필요)
        //aiProcess_GenNormals              // 노말이 없으면 자동 생성
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
    for (unsigned int i = 0; i < scene->mNumMaterials && i < 8; i++)
    {
        aiMaterial* material = scene->mMaterials[i];        
        meshStoreBox->AddMaterial(material);
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
    // 1. 살아있는 face들을 subMeshID 별로 그룹핑
    // ─────────────────────────────────────────────         

    std::map<int, std::vector<QEMFace*>> bySubMesh;
    for (auto* f : storeBox->GetFaces()) {
        if (!f || f->isDeleted) continue;
        if (!f->indices[0] || !f->indices[1] || !f->indices[2]) continue;
        if (f->indices[0] == f->indices[1] ||
            f->indices[1] == f->indices[2] ||
            f->indices[0] == f->indices[2]) continue;

        bySubMesh[f->metaData.subMeshID].push_back(f);
    }

    if (bySubMesh.empty()) {
        LOG_ERROR("ExportModel: no live faces to export");
        return false;
    }

    // ─────────────────────────────────────────────
    // 2. aiScene / 머티리얼 / 루트 노드 골격
    // ─────────────────────────────────────────────
    aiScene* scene = new aiScene();
    scene->mRootNode = new aiNode();

    const unsigned int meshCount = static_cast<unsigned int>(bySubMesh.size());

    scene->mNumMeshes = meshCount;
    scene->mMeshes = new aiMesh * [meshCount];

    scene->mNumMaterials = max(1u, meshCount);
    scene->mMaterials = new aiMaterial * [scene->mNumMaterials];
    for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
        scene->mMaterials[i] = new aiMaterial();
        aiString matName(("Material_" + std::to_string(i)).c_str());
        scene->mMaterials[i]->AddProperty(&matName, AI_MATKEY_NAME);
    }

    scene->mRootNode->mNumMeshes = meshCount;
    scene->mRootNode->mMeshes = new unsigned int[meshCount];

    // ─────────────────────────────────────────────
    // 3. 서브메시별 aiMesh 빌드
    // ─────────────────────────────────────────────

    // UV Seam 처리를 위한 vertex split 키 타입.
    // 같은 QEMVertex*라도 UV가 다르면 별도 로컬 인덱스를 부여한다.
    //
    // UV 비교는 채널별 (u, v) 값을 비트 단위로 직접 비교한다.
    // float 오차 허용이 필요하면 quantize 함수를 끼워 넣으면 된다.
    struct UVKey
    {
        // 채널별 UV를 int32로 quantize하여 해시에 사용
        // (float을 그대로 비교하면 QEM 보간 후 미세한 차이로 seam이
        //  과도하게 split되므로, 허용 오차 epsilon 안에서는 같은 키로 묶는다)
        //static const float UV_QUANTIZE = 1e-4f; // seam 판정 허용 오차

        QEMVertex* ptr;
        std::vector<std::pair<int, std::pair<int32_t, int32_t>>> quantizedUVs; // channel → (qu, qv)

        bool operator==(const UVKey& o) const
        {
            if (ptr != o.ptr) return false;
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
            for (const auto& [ch, quv] : k.quantizedUVs)
            {
                h ^= std::hash<int>()(ch) * 2654435761u;
                h ^= std::hash<int32_t>()(quv.first) * 40503u;
                h ^= std::hash<int32_t>()(quv.second) * 12345u;
            }
            return h;
        }
    };

    // face corner의 UV map으로부터 UVKey 생성 헬퍼
    auto makeUVKey = [](QEMVertex* v, const std::map<int, UVElement>& uvMap) -> UVKey
        {
            UVKey key;
            key.ptr = v;
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
        const int subID = kv.first;
        std::vector<QEMFace*>& groupFaces = kv.second;
        aiMesh* m = new aiMesh();

        int actualMatIdx = groupFaces[0]->metaData.materialIndex;
        if (actualMatIdx < 0 || actualMatIdx >= (int)scene->mNumMaterials)
            m->mMaterialIndex = 0;
        else
            m->mMaterialIndex = (unsigned int)actualMatIdx;

        // ── 핵심 수정: (QEMVertex* + UV snapshot) 조합을 키로 사용 ──
        // 같은 정점 포인터라도 UV가 다르면 split하여 별도 로컬 인덱스 부여.
        // 이를 통해 UV Seam에서 last-write-wins로 UV가 덮어씌워지던 버그를 제거한다.
        std::unordered_map<UVKey, unsigned int, UVKeyHash> splitMap; // (ptr+UV) → localIdx
        std::vector<QEMVertex*>                             localVerts;
        std::vector<std::map<int, UVElement>>               localUvs;

        // face index 배열 (face별 코너 3개의 로컬 인덱스를 미리 저장해 face 루프에서 재사용)
        std::vector<std::array<unsigned int, 3>> faceLocalIndices;
        faceLocalIndices.reserve(groupFaces.size());

        for (auto* f : groupFaces)
        {
            std::array<unsigned int, 3> corners{};
            for (int k = 0; k < 3; ++k)
            {
                QEMVertex* v = f->indices[k];
                UVKey ukey = makeUVKey(v, f->uvs[k]);

                auto it = splitMap.find(ukey);
                if (it == splitMap.end())
                {
                    // 새 조합 → 새 로컬 슬롯 할당
                    unsigned int newIdx = static_cast<unsigned int>(localVerts.size());
                    splitMap[ukey] = newIdx;
                    localVerts.push_back(v);
                    localUvs.push_back(f->uvs[k]); // 이 코너의 UV를 그대로 저장
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

            switch (coordinate)
            {
            case CoordinateFlag::Unity:
                m->mVertices[i] = aiVector3D(x, y, -z);
                m->mNormals[i] = aiVector3D(0, 1, 0);
                break;
            case CoordinateFlag::Unreal:
                m->mVertices[i] = aiVector3D(-z, x, y);
                m->mNormals[i] = aiVector3D(0, 0, 1);
                break;
            case CoordinateFlag::Source:
            default:
                m->mVertices[i] = aiVector3D(x, y, z);
                m->mNormals[i] = aiVector3D(0, 1, 0);
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

        scene->mMeshes[meshSlot] = m;
        scene->mRootNode->mMeshes[meshSlot] = meshSlot;
        ++meshSlot;
    }

    scene->mRootNode->mName.Set("Root");

    // ─────────────────────────────────────────────
    // 4. FBX 저장
    // ─────────────────────────────────────────────
    try
    {
        LOG_INFO("export path : {}{}", filePath, path);
        Assimp::Exporter exporter;
        aiReturn res = exporter.Export(scene, "fbx", filePath + path);
        if (res != aiReturn_SUCCESS)
            LOG_ERROR("Export failed: {}", exporter.GetErrorString());
        delete scene;
        return (res == aiReturn_SUCCESS);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Error in Export : {}", e.what());
        delete scene;
        return false;
    }
}

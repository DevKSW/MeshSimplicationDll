#include "pch.h"
#include "MeshStoreBox.h"
#include <algorithm>
#include "Logger.h"
using namespace DirectX;

QEMVertex& ::MeshStoreBox::AddVertex(const QEMVertex& vertex)
{
    QEMVertex* v = new QEMVertex(vertex);
    v->position.w = 1.0f;
    this->vertices.push_back(v);
    return *v;
}

void::MeshStoreBox::AddFace(const QEMFace& face)
{   
    this->faces.push_back(new QEMFace(face));
}

void MeshStoreBox::AddMaterial(aiMaterial* mat)
{
    aiMaterial* newMat = new aiMaterial();
    aiMaterial::CopyPropertyList(newMat, mat);

    // 임베디드 텍스처 참조 검사 + 정화
    aiString texPath;
    for (aiTextureType type : {aiTextureType_DIFFUSE, aiTextureType_BASE_COLOR,
        aiTextureType_NORMALS, aiTextureType_SPECULAR}) 
    {
        if (newMat->GetTexture(type, 0, &texPath) == AI_SUCCESS) 
        {
            std::string p = texPath.C_Str();
            if (!p.empty() && p[0] == '*') 
            {
                // 임베디드 참조 → 빈 경로로 교체 (또는 별도 파일로 추출 후 경로 변경)
                LOG_WARN("Material has embedded texture ref '{}', clearing", p);
                aiString empty;
                newMat->AddProperty(&empty, _AI_MATKEY_TEXTURE_BASE, type, 0);
            }
        }
    }

    // 머티리얼 이름 누락 방지
    aiString matName;
    if (newMat->Get(AI_MATKEY_NAME, matName) != AI_SUCCESS) {
        std::string fallback = "Material_" + std::to_string(materials.size());
        aiString fixedName(fallback.c_str());
        newMat->AddProperty(&fixedName, AI_MATKEY_NAME);
    }

    materials.push_back(newMat);
}



QEMVertex* MeshStoreBox::FindVertex(QEMVertex vertex)
{
    QEMVertex* ptr = nullptr;
    
    for (auto iter = vertices.begin(); iter != vertices.end(); iter++)
    {
        if (vertex == *(*iter))
        {
            ptr = *iter;
            break;
        }
    }

    return ptr;
}

QEMFace* MeshStoreBox::FindFace(QEMFace face)
{
    QEMFace* ptr = nullptr;

    for (auto iter = faces.begin(); iter != faces.end(); iter++)
    {
        if (face == *(*iter))        
        {
            ptr = *iter;
            break;
        }
    }

    return ptr;
}


void MeshStoreBox::Clear()
{
    for (auto iter = vertices.begin(); iter != vertices.end(); iter++)
    {
        QEMVertex* ptr = (*iter);
        delete ptr;
        ptr = nullptr;
    }
    for (auto iter = faces.begin(); iter != faces.end(); iter++)
    {
        QEMFace* ptr = (*iter);
        delete ptr;
        ptr = nullptr;
    }
    for (auto mat : materials) delete mat;

    vertices.clear();
    faces.clear();
    materials.clear();
}

void MeshStoreBox::InitElements()
{
    for (auto iter = faces.begin(); iter != faces.end(); iter++)
    {
        QEMFace* face = (*iter);
        if (face->indices[0] == nullptr || face->indices[1] == nullptr || face->indices[2] == nullptr)
        {          
            delete face;
            face = nullptr;
            continue;
        }

        face->indices[0]->adjacentVertices.insert(face->indices[1]);
        face->indices[0]->adjacentVertices.insert(face->indices[2]);
        face->indices[0]->adjacentFaces.insert(face);

        face->indices[1]->adjacentVertices.insert(face->indices[0]);
        face->indices[1]->adjacentVertices.insert(face->indices[2]);
        face->indices[1]->adjacentFaces.insert(face);

        face->indices[2]->adjacentVertices.insert(face->indices[0]);
        face->indices[2]->adjacentVertices.insert(face->indices[1]);
        face->indices[2]->adjacentFaces.insert(face);        

        face->plane = ComputePlane(face->indices[0], face->indices[1], face->indices[2]);

        //std::cout << "MeshStoreBox::InitElements()\n"
        //    << "\t face.indices[0]'s connected Vertex Cnt = " << face->indices[0]->adjacentVertices.size()
        //    << "\n\t face.indices[1]'s connected Vertex Cnt = " << face->indices[1]->adjacentVertices.size()
        //    << "\n\t face.indices[2]'s connected Vertex Cnt = " << face->indices[2]->adjacentVertices.size() 
        //    <<"\n plane : " << face->plane.x << " , " << face->plane.y << " , " << face->plane.z << " , " << std::endl;
    }
}

void MeshStoreBox::ComputePlanes()
{
    for (auto iter = faces.begin(); iter != faces.end(); iter++)
    {       
        (*iter)->plane = ComputePlane((*iter)->indices[0], (*iter)->indices[1],(*iter)->indices[2]);        
    }
}

void MeshStoreBox::RemoveDeletedElements()
{
    // 1. 삭제 마킹된 Face들의 메모리를 반환하고 벡터에서 제거
    faces.erase(
        std::remove_if(faces.begin(), faces.end(), [](QEMFace* face) {
            if (face->isDeleted)
            {
                delete face;
                face = nullptr;// 동적 할당된 메모리 해제
                return true; // 벡터에서 제거 대상
            }
            return false;    // 유지 대상
            }),
        faces.end()
    );

    // 2. 삭제 마킹된 Vertex들의 메모리를 반환하고 벡터에서 제거
    vertices.erase(
        std::remove_if(vertices.begin(), vertices.end(), [](QEMVertex* vertex) {
            if (vertex->isDeleted)
            {
                delete vertex; // 동적 할당된 메모리 해제
                vertex = nullptr;
                return true;   // 벡터에서 제거 대상
            }
            return false;      // 유지 대상
            }),
        vertices.end()
    );
}

MeshStoreBox::~MeshStoreBox()
{
    this->Clear();
}

XMFLOAT4 MeshStoreBox::ComputePlane(const QEMVertex* v0, const QEMVertex* v1, const QEMVertex* v2)
{
    // 세 정점의 위치를 로드
    if (v0 == nullptr || v1 == nullptr || v2 == nullptr) return XMFLOAT4(.0f,.0f,.0f,.0f);

    XMVECTOR p0 = XMLoadFloat4(&v0->position);
    XMVECTOR p1 = XMLoadFloat4(&v1->position);
    XMVECTOR p2 = XMLoadFloat4(&v2->position);

    // 두 엣지 벡터
    XMVECTOR edge1 = XMVectorSubtract(p1, p0);
    XMVECTOR edge2 = XMVectorSubtract(p2, p0);

    // 법선 = edge1 × edge2 (정규화)
    XMVECTOR normal = XMVector3Normalize(XMVector3Cross(edge1, edge2));
    if (XMVector3Equal(normal, XMVectorZero()) ||
        std::isnan(XMVectorGetX(normal)))
        return XMFLOAT4(0, 0, 0, 0);

    // d = -(normal · p0)
    float d = -XMVectorGetX(XMVector3Dot(normal, p0));

    XMFLOAT4 plane;
    XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(&plane), normal);
    plane.w = d;

    return plane;
}
int MeshStoreBox::WeldVerticesByPosition(float epsilon)
{
    using namespace DirectX;
    LOG_INFO("[Weld] start: V={}, eps={:.3e}", vertices.size(), epsilon);

    if (vertices.empty() || epsilon <= 0.0f) {
        LOG_WARN("[Weld] skip: empty or invalid epsilon");
        return 0;
    }

    // overflow 방어: int64로 quantize
    auto quantize = [eps = epsilon](const XMFLOAT4& p) -> WeldKey {
        return WeldKey{
            (int)std::floor(p.x / eps),
            (int)std::floor(p.y / eps),
            (int)std::floor(p.z / eps)
        };
        };

    const float eps2 = epsilon * epsilon;

    // 1. grid 빌드
    std::unordered_map<WeldKey, std::vector<QEMVertex*>, WeldKeyHash> grid;
    grid.reserve(vertices.size() * 2);
    for (auto* v : vertices) {
        if (!v) continue;            // null 방어
        if (v->isDeleted) continue;
        grid[quantize(v->position)].push_back(v);
    }
    LOG_INFO("[Weld] grid built: {} cells", grid.size());

    // 2. remap 결정 — 모든 살아있는 vertex가 remap에 들어가도록 보장
    std::unordered_map<QEMVertex*, QEMVertex*> remap;
    remap.reserve(vertices.size());

    for (auto* v : vertices) {
        if (!v) continue;
        if (v->isDeleted) continue;
        if (remap.count(v)) continue;

        remap[v] = v;  // 자기 자신을 survivor로 등록 (절대 누락 안 됨)

        WeldKey base = quantize(v->position);
        XMVECTOR pv = XMLoadFloat4(&v->position);

        for (int dz = -1; dz <= 1; ++dz)
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                {
                    WeldKey key{ base.x + dx, base.y + dy, base.z + dz };
                    auto it = grid.find(key);
                    if (it == grid.end()) continue;

                    for (auto* u : it->second) {
                        if (u == v || !u || u->isDeleted) continue;
                        if (remap.count(u)) continue;

                        XMVECTOR pu = XMLoadFloat4(&u->position);
                        XMVECTOR d = XMVectorSubtract(pu, pv);
                        if (XMVectorGetX(XMVector3LengthSq(d)) < eps2)
                            remap[u] = v;
                    }
                }
    }

    // 2.5. remap 완전성 검증 (디버그용, 안정화 후 제거)
    //int unmapped = 0;
    //for (auto* v : vertices) {
    //    if (!v || v->isDeleted) continue;
    //    if (!remap.count(v)) unmapped++;
    //}
    //if (unmapped > 0) {
    //    LOG_ERROR("[Weld] {} vertices unmapped — aborting weld", unmapped);
    //    return 0;
    //}

    // 3. face indices 업데이트 (find로 안전 조회)
    int faceRefsUpdated = 0;
    int badFaces = 0;
    for (auto* f : faces) {
        if (!f || f->isDeleted) continue;
        for (int i = 0; i < 3; ++i) {
            if (!f->indices[i]) { badFaces++; continue; }
            auto it = remap.find(f->indices[i]);
            if (it == remap.end()) {
                LOG_WARN("[Weld] face has off-grid vertex");
                badFaces++;
                continue;
            }
            if (it->second != f->indices[i]) {
                f->indices[i] = it->second;
                ++faceRefsUpdated;
            }
        }
        // degenerate 후처리
        if (f->indices[0] == f->indices[1] ||
            f->indices[1] == f->indices[2] ||
            f->indices[0] == f->indices[2]) {
            f->isDeleted = true;
        }
    }

    // 4. 흡수 vertex 마킹
    int weldedVerts = 0;
    for (auto* v : vertices) {
        if (!v || v->isDeleted) continue;
        auto it = remap.find(v);
        if (it != remap.end() && it->second != v) {
            v->isDeleted = true;
            ++weldedVerts;
        }
    }

    LOG_INFO("[Weld] done: welded={}, faceRefsUpdated={}, badFaces={}",
        weldedVerts, faceRefsUpdated, badFaces);

    RemoveDeletedElements();
    LOG_INFO("[Weld] after compact: V={}, F={}", vertices.size(), faces.size());

    return weldedVerts;
}

#include "pch.h"
#include "MeshStoreBox.h"
#include <iostream>

using namespace DirectX;

void::MeshStoreBox::AddVertex(const QEMVertex& vertex)
{
    this->vertices.push_back(new QEMVertex(vertex));
}

void::MeshStoreBox::AddFace(const QEMFace& face)
{   
    this->faces.push_back(new QEMFace(face));
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

    return nullptr;
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

    vertices.clear();
    faces.clear();
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

        face->plane = ComputePlane(*face->indices[0], *face->indices[1], *face->indices[2]);

        std::cout << "MeshStoreBox::InitElements()\n"
            << "\t face.indices[0]'s connected Vertex Cnt = " << face->indices[0]->adjacentVertices.size()
            << "\n\t face.indices[1]'s connected Vertex Cnt = " << face->indices[1]->adjacentVertices.size()
            << "\n\t face.indices[2]'s connected Vertex Cnt = " << face->indices[2]->adjacentVertices.size() 
            <<"\n plane : " << face->plane.x << " , " << face->plane.y << " , " << face->plane.z << " , " << std::endl;
    }
}

void MeshStoreBox::ComputePlanes()
{
    for (auto iter = faces.begin(); iter != faces.end(); iter++)
    {       
        (*iter)->plane = ComputePlane(*(*iter)->indices[0], *(*iter)->indices[1], *(*iter)->indices[2]);
    }
}

MeshStoreBox::~MeshStoreBox()
{
    this->Clear();
}

XMFLOAT4 MeshStoreBox::ComputePlane(const QEMVertex& v0, const QEMVertex& v1, const QEMVertex& v2)
{
    // 세 정점의 위치를 로드
    if (&v0 == nullptr || &v1 == nullptr || &v2 == nullptr) return XMFLOAT4(.0f,.0f,.0f,.0f);

    XMVECTOR p0 = XMLoadFloat4(&v0.position);
    XMVECTOR p1 = XMLoadFloat4(&v1.position);
    XMVECTOR p2 = XMLoadFloat4(&v2.position);

    // 두 엣지 벡터
    XMVECTOR edge1 = XMVectorSubtract(p1, p0);
    XMVECTOR edge2 = XMVectorSubtract(p2, p0);

    // 법선 = edge1 × edge2 (정규화)
    XMVECTOR normal = XMVector3Normalize(XMVector3Cross(edge1, edge2));

    // d = -(normal · p0)
    float d = -XMVectorGetX(XMVector3Dot(normal, p0));

    XMFLOAT4 plane;
    XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(&plane), normal);
    plane.w = d;

    return plane;
}

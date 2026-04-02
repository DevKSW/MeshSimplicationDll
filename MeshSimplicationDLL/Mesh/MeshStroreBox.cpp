#include "pch.h"
#include "MeshStroreBox.h"

using namespace DirectX;

void MeshStoreBox::BuildAdjacency()
{
    for (uint32_t fIdx = 0; fIdx < static_cast<uint32_t>(faces.size()); fIdx++)
    {
        QEMFace& face = faces[fIdx];

        // 평면 방정식 계산
        face.plane = ComputePlane(
            vertices[face.indices[0]],
            vertices[face.indices[1]],
            vertices[face.indices[2]]
        );

        // 각 정점에 인접 면 등록 + 정점 간 인접 관계 등록
        for (int i = 0; i < 3; i++)
        {
            uint32_t vi = face.indices[i];
            vertices[vi].adjacentFaces.insert(fIdx);

            for (int j = 0; j < 3; j++)
            {
                if (i != j)
                {
                    vertices[vi].adjacentVertices.insert(face.indices[j]);
                }
            }
        }
    }
}

void MeshStoreBox::Clear()
{
    vertices.clear();
    faces.clear();
}

XMFLOAT4 MeshStoreBox::ComputePlane(const QEMVertex& v0, const QEMVertex& v1, const QEMVertex& v2)
{
    // 세 정점의 위치를 로드
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

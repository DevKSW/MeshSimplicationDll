#pragma once
#include <DirectXMath.h>
#include <vector>
#include <unordered_set>

#ifndef APPROXY 
#define APPROXY 0.01f
#endif 

struct QEMFace;
// ─────────────────────────────────────────────
// QEM 정점 (Vertex)
// ─────────────────────────────────────────────
struct QEMVertex
{
    DirectX::XMFLOAT4 position;              // (x, y, z, 1) 동차좌표

    DirectX::XMFLOAT4X4 Q;                   // 이 정점의 Q 행렬 (인접 면들의 Kp 합)

    std::unordered_set<QEMFace*> adjacentFaces;     // 이 정점이 속한 삼각형 
    std::unordered_set<QEMVertex*> adjacentVertices;  // 이웃 정점 

    QEMVertex()
        : position{ 0, 0, 0, 1 }, Q{} {}

    QEMVertex(float x, float y, float z)
        : position{ x, y, z, 1.0f }, Q{} {}

    bool operator==(QEMVertex& other) const
    {
        return (this->position.x - other.position.x < APPROXY &&
                this->position.y - other.position.y < APPROXY &&
                this->position.z - other.position.z < APPROXY);
    }

};

// ─────────────────────────────────────────────
// QEM 면 (Face)
// ─────────────────────────────────────────────
struct QEMFace
{
    QEMVertex* indices[3];                      // 세 정점
    DirectX::XMFLOAT4 plane;                  // 평면 방정식 (a, b, c, d) → Kp 계산용    
    //DirectX::XMFLOAT4 normal;

    QEMFace()
        : indices{ 0, 0, 0 }, plane{ 0, 0, 0, 0 } {}

    QEMFace(QEMVertex* i0, QEMVertex* i1, QEMVertex* i2)
        : indices{ i0, i1, i2 }, plane{ 0, 0, 0, 0 } {}

    bool operator==(QEMFace& other) const
    {
        return 
            (this->indices[0] == other.indices[0] && 
            this->indices[1] == other.indices[1] &&
            this->indices[2] == other.indices[2] );
    }

};

// ─────────────────────────────────────────────
// QEM 엣지 축약 후보 (Pair)
// ─────────────────────────────────────────────
struct QEMPair
{
    QEMVertex* v1, * v2;                          // 두 정점
    DirectX::XMFLOAT4 optimalPos;             // 최적 축약 위치
    double cost;                              // v^T * (Q1+Q2) * v

    // 우선순위 큐 (min-heap) 용 비교
    bool operator>(const QEMPair& other) const {
        return cost > other.cost;
    }
};

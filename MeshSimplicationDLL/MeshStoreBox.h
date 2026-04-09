#pragma once
#include "CustomStructures.h"
#include <vector>

/// 파싱된 메시 데이터를 보관하고 QEM에 필요한 자료구조를 제공
class MeshStoreBox
{
private:
    std::vector<QEMVertex*> vertices;
    std::vector<QEMFace*>   faces;

public:
    void AddVertex(const QEMVertex& vertex);
    void AddFace(const QEMFace& face);  

    QEMVertex* FindVertex(QEMVertex vertex);
    QEMFace* FindFace(QEMFace face);

    /// 모든 데이터 초기화
    void Clear();

    uint32_t GetVertexCount() const { return static_cast<uint32_t>(vertices.size()); }
    uint32_t GetFaceCount()   const { return static_cast<uint32_t>(faces.size()); }

    const std::vector<QEMVertex*>& GetVertices() const { return vertices; }
    const std::vector<QEMFace*>& GetFaces() const { return faces; }

    void InitElements();
    void ComputePlanes();


    ~MeshStoreBox();

private:
    /// 삼각형 세 정점으로부터 평면 방정식 (a, b, c, d) 계산
    DirectX::XMFLOAT4 ComputePlane(const QEMVertex& v0, const QEMVertex& v1, const QEMVertex& v2);
};

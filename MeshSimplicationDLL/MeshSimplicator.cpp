#include "pch.h"
#include "MeshSimplicator.h"
#include "MeshStoreBox.h"
#include "ModelParser.h"

#include <iostream>

MeshSimplicator::MeshSimplicator()
{
	this->parser = new ModelParser();
	this->storeBox = nullptr;
	this->simplicateLevel = 5;	
}

bool MeshSimplicator::LoadModel(std::string path)
{
	if (parser == nullptr)
		parser = new ModelParser();


	MeshStoreBox* box =  parser->LoadModel(path);
	if (box == nullptr)
		return false;

	storeBox = box;

	//Debug Log 
	std::cout << "Load Complete! : \n"
		<< "Vertex Count : " << storeBox->GetVertexCount()
		<< "\nFace Count : " << storeBox->GetFaceCount();

	storeBox->InitElements();

	// QEM 초기화 로직 수행
	ComputeVertexQMatrices();
	GenerateInitialPairs();
	
	std::cout << "\nPairs generated: " << pairsQueue.size();
	if (!pairsQueue.empty())
	{
		std::cout << "\nLowest cost pair: " << pairsQueue.top().cost << std::endl;
	}

	return true;
}

void MeshSimplicator::Simplicate()
{	
	for (int i = 0; i < simplicateLevel; ++i)
	{
		


	}
}

void MeshSimplicator::ComputeVertexQMatrices()
{
	auto& vertices = storeBox->GetVertices();
	for (auto* vertex : vertices)
	{
		// Q 행렬 0으로 초기화
		DirectX::XMStoreFloat4x4(&vertex->Q, DirectX::XMMatrixSet(
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0
		));
		
		DirectX::XMMATRIX vertexQ = DirectX::XMLoadFloat4x4(&vertex->Q);

		for (auto* face : vertex->adjacentFaces)
		{
			// 평면 방정식 (a, b, c, d)
			float a = face->plane.x;
			float b = face->plane.y;
			float c = face->plane.z;
			float d = face->plane.w;
			
			// Kp = p * p^T
			DirectX::XMMATRIX Kp = DirectX::XMMatrixSet(
				a*a, a*b, a*c, a*d,
				a*b, b*b, b*c, b*d,
				a*c, b*c, c*c, c*d,
				a*d, b*d, c*d, d*d
			);

			vertexQ = vertexQ + Kp;
		}
		
		DirectX::XMStoreFloat4x4(&vertex->Q, vertexQ);
	}
}

void MeshSimplicator::ComputePairCostAndPos(QEMPair& p)
{
	using namespace DirectX;

	XMMATRIX q1 = XMLoadFloat4x4(&p.v1->Q);
	XMMATRIX q2 = XMLoadFloat4x4(&p.v2->Q);
	XMMATRIX Q_bar = q1 + q2;

	// 최적화 대상 행렬 M 제작 
	// (Q_bar의 상단 3x3 부분과 마지막 행 [0, 0, 0, 1] 결합)
	XMFLOAT4X4 m_data;
	XMStoreFloat4x4(&m_data, Q_bar);
	m_data._41 = 0.0f;
	m_data._42 = 0.0f;
	m_data._43 = 0.0f;
	m_data._44 = 1.0f;

	XMMATRIX M = XMLoadFloat4x4(&m_data);
	
	// M 역행렬 도출 시도
	XMVECTOR det;
	XMMATRIX M_inv = XMMatrixInverse(&det, M);
	
	bool isSingular = XMVectorGetX(det) == 0.0f || std::isinf(XMVectorGetX(det)) || std::isnan(XMVectorGetX(det));
	
	XMVECTOR pos;
	
	auto computeCost = [&](XMVECTOR v) -> double {
		// v^T * Q_bar * v
		XMVECTOR Q_bar_v = XMVector4Transform(v, Q_bar);
		XMVECTOR vT_Q_bar_v = XMVector4Dot(v, Q_bar_v);
		return static_cast<double>(XMVectorGetX(vT_Q_bar_v));
	};

	if (!isSingular)
	{
		// 역행렬이 존재하면 최적 위치 = M_inv * [0,0,0,1]^T 
		// (본질적으로 M_inv의 4번째 열)
		pos = XMVectorSet(
			XMVectorGetX(M_inv.r[3]), 
			XMVectorGetY(M_inv.r[3]), 
			XMVectorGetZ(M_inv.r[3]), 
			1.0f);
	}
	else
	{
		// Fallback 전략: v1, v2, 그 중간점 중 에러가 제일 적은 곳 선택
		XMVECTOR v1pos = XMLoadFloat4(&p.v1->position);
		XMVECTOR v2pos = XMLoadFloat4(&p.v2->position);
		XMVECTOR midpos = XMVectorScale(XMVectorAdd(v1pos, v2pos), 0.5f);
		
		// 스케일 이후 w성분이 0.5f가 되므로 다시 1.0f로 강제 세팅 (동차좌표 대응)
		midpos = XMVectorSetW(midpos, 1.0f);
		
		double err1 = computeCost(v1pos);
		double err2 = computeCost(v2pos);
		double err3 = computeCost(midpos);

		double minErr = min(err1, min(err2, err3));
		if (minErr == err1) pos = v1pos;
		else if (minErr == err2) pos = v2pos;
		else pos = midpos;
	}

	XMStoreFloat4(&p.optimalPos, pos);
	p.cost = computeCost(pos);
}

void MeshSimplicator::GenerateInitialPairs()
{
	std::cout << "MeshSimplicator::GenerateInitialPairs() \n";

	// 큐 초기화 (swap 트릭)
	std::priority_queue<QEMPair, std::vector<QEMPair>, std::greater<QEMPair>> empty_queue;
	std::swap(pairsQueue, empty_queue);

	auto& vertices = storeBox->GetVertices();
	for (auto* v1 : vertices)
	{
		//std::cout << "v1 : ( " << v1->position.x << " , " << v1->position.y << " , " << v1->position.z << " ) connected : "<< v1->adjacentVertices.size() << std::endl;
		for (auto* v2 : v1->adjacentVertices)
		{
			// v1 < v2인 경우만 계산하여 중복(v2->v1) 처리 방지
			if (v1 < v2)
			{				
				QEMPair cp;
				cp.v1 = v1;
				cp.v2 = v2;
				ComputePairCostAndPos(cp);
				pairsQueue.push(cp);
			}
		}
	}
}

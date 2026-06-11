#pragma once
#include <vector>
#include <queue>
#include "CustomStructures.h"

class ModelParser;
class MeshStoreBox;

class MeshSimplicator
{
private :
	ModelParser* parser;
	MeshStoreBox* storeBox;
	float boundaryWeight;

	CoordinateFlag coord;

	std::priority_queue<QEMPair, std::vector<QEMPair>, std::greater<QEMPair>> pairsQueue;
	
	std::string fileName;
	std::string filePath;
	int simplicateLevel;
	int simplicateRate;

	void ComputeVertexQMatrices();
	void AddBoundaryConstraints();
	void GenerateInitialPairs();
	void ComputePairCostAndPos(QEMPair& p);
	bool IsCollapseValid(QEMVertex* v1, QEMVertex* v2,const DirectX::XMFLOAT4& newPos,float minDot = 0.0f);
	bool TryGetSafePosition(QEMPair& p, DirectX::XMFLOAT4& outPos);

public :
	
	MeshSimplicator();
	~MeshSimplicator();

	bool LoadModel(std::string path);
	
	void Simplicate();

	void SetCoordinate(CoordinateFlag flag);

	void Reset();

	bool CheckIfUVSeam(QEMVertex* va, QEMVertex* vb, QEMFace* f1, QEMFace* f2);


public :
	void SetSimplicateLevel(int level) { simplicateLevel = level; }
	void SetSimplicateRate(int rate) { simplicateRate = rate; }

};


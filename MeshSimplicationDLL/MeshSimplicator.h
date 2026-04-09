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
	
	std::priority_queue<QEMPair, std::vector<QEMPair>, std::greater<QEMPair>> pairsQueue;
	
	int simplicateLevel;

	void ComputeVertexQMatrices();
	void GenerateInitialPairs();
	void ComputePairCostAndPos(QEMPair& p);

public :
	
	MeshSimplicator();

	bool LoadModel(std::string path);
	
	void Simplicate();


public :
	void SetSimplicateLevel(int level) { simplicateLevel = level; }

};


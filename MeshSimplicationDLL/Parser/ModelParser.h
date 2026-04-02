#pragma once
#include "../Mesh/MeshStroreBox.h"

class ModelParser
{	
	Assimp::Importer importer;

public: 
	/// 모델 파일을 로드하여 MeshStoreBox에 정점/면 데이터 저장
	void LoadModel(const std::string& path, MeshStoreBox& outMesh);

};

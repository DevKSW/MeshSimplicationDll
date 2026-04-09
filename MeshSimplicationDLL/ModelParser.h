#pragma once
#include "pch.h"

class MeshStoreBox;

class ModelParser
{	
	Assimp::Importer importer;

public: 
	/// 모델 파일을 로드하여 MeshStoreBox에 정점/면 데이터 저장
	MeshStoreBox* LoadModel(const std::string& path);
	
	/// MeshStoreBox 데이터를 FBX(기타 포맷)로 내보내기
	bool ExportModel(MeshStoreBox* storeBox, const std::string& path);
};

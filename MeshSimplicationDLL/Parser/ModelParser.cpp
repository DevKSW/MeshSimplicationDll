#include "pch.h"
#include "ModelParser.h"
#include "../Mesh/MeshStoreBox.h"

MeshStoreBox* ModelParser::LoadModel(std::string& path)
{	   
    MeshStoreBox* meshStoreBox = new MeshStoreBox();

    try
    {
    const aiScene* scene = this->importer.ReadFile(path,
        aiProcess_Triangulate |           // 모든 면을 삼각형으로 변환
        aiProcess_FlipUVs |               // DirectX/OpenGL 좌표계 차이 해결
        aiProcess_JoinIdenticalVertices | // 중복 정점 제거 (QEM 준비 단계에서 필요)
        aiProcess_GenNormals              // 노말이 없으면 자동 생성
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string errorStr = importer.GetErrorString();
        throw std::exception(errorStr.c_str()); // 모델 실패 , 메쉬 미존재 시 Throw
    }

    // 메시 순회
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[i];

        // Exctract vertex data
        for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
            aiVector3D pos = mesh->mVertices[j];
            // 여기서 정점을 구조체에 저장
            meshStoreBox
        }

        // Exctract face(index) data
        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            aiFace face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; k++) {
                unsigned int index = face.mIndices[k];
                // 삼각형 인덱스 저장
            }
        }
    }    

    }
    catch (const std::exception& e)
    {        
        std::cerr << "Error: " << e.what() << std::endl;        
        delete meshStoreBox;
    }    

    return meshStoreBox;
}

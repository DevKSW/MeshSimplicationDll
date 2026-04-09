#include "pch.h"
#include "ModelParser.h"
#include "MeshStoreBox.h"
#include <iostream>


MeshStoreBox* ModelParser::LoadModel(const std::string& path)
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
            meshStoreBox->AddVertex(QEMVertex(pos.x, pos.y, pos.z));
        }

        // Exctract face(index) data
        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            aiFace face = mesh->mFaces[j];            

            QEMFace qemFace;

            if (face.mNumIndices != 3) continue;          
            auto vertex = mesh->mVertices[face.mIndices[0]];
            qemFace.indices[0] = meshStoreBox->FindVertex(QEMVertex(vertex.x, vertex.y, vertex.z));
            vertex = mesh->mVertices[face.mIndices[1]];
            qemFace.indices[1] = meshStoreBox->FindVertex(QEMVertex(vertex.x, vertex.y, vertex.z));
            vertex = mesh->mVertices[face.mIndices[2]];
            qemFace.indices[2] = meshStoreBox->FindVertex(QEMVertex(vertex.x, vertex.y, vertex.z));            
            //face = QEMFace(face.mIndices[0], face.mIndices[1], face.mIndices[2]);
            meshStoreBox->AddFace(qemFace);
        }
    }    

    }
    catch (const std::exception& e)
    {        
        std::cerr << "Error: " << e.what() << std::endl;        
        delete meshStoreBox;
        meshStoreBox = nullptr;        
    }

    return meshStoreBox;
}

bool ModelParser::ExportModel(MeshStoreBox* storeBox, const std::string& path)
{
    if (!storeBox) return false;

    // 1. aiScene 및 RootNode 생성
    aiScene* scene = new aiScene();
    scene->mRootNode = new aiNode();
    
    // 2. aiMesh 생성
    aiMesh* mesh = new aiMesh();
    mesh->mMaterialIndex = 0;

    const auto& vertices = storeBox->GetVertices();
    const auto& faces = storeBox->GetFaces();

    mesh->mNumVertices = static_cast<unsigned int>(vertices.size());
    mesh->mVertices = new aiVector3D[mesh->mNumVertices];
    // Normals array 생성 (필수가 아닐 수 있으나 보장 차원에서 생성)
    mesh->mNormals = new aiVector3D[mesh->mNumVertices];

    // QEMVertex 포인터를 인덱스 번호로 변환하기 위한 맵
    std::unordered_map<QEMVertex*, unsigned int> vertexToIdxMap;
    
    // 정점 데이터 복사
    for (size_t i = 0; i < vertices.size(); i++) {
        mesh->mVertices[i] = aiVector3D(vertices[i]->position.x, vertices[i]->position.y, vertices[i]->position.z);
        // 법선 데이터가 있다면 여기에 채울 수 있음. 임시로 초기화
        mesh->mNormals[i] = aiVector3D(0.0f, 1.0f, 0.0f);
        vertexToIdxMap[vertices[i]] = static_cast<unsigned int>(i);
    }

    // 3. 면 데이터 복사
    mesh->mNumFaces = static_cast<unsigned int>(faces.size());
    mesh->mFaces = new aiFace[mesh->mNumFaces];

    for (size_t i = 0; i < faces.size(); i++) {
        aiFace& face = mesh->mFaces[i];
        face.mIndices = new unsigned int[3];
        face.mNumIndices = 3;
        
        face.mIndices[0] = vertexToIdxMap[faces[i]->indices[0]];
        face.mIndices[1] = vertexToIdxMap[faces[i]->indices[1]];
        face.mIndices[2] = vertexToIdxMap[faces[i]->indices[2]];
    }

    // 4. Scene에 Mesh 등록
    scene->mNumMeshes = 1;
    scene->mMeshes = new aiMesh*[1];
    scene->mMeshes[0] = mesh;

    // RootNode에 포함할 Mesh 인덱스 등록
    scene->mRootNode->mNumMeshes = 1;
    scene->mRootNode->mMeshes = new unsigned int[1];
    scene->mRootNode->mMeshes[0] = 0;

    // Assimp Export 시 Material 정보가 1개 이상 필요
    scene->mNumMaterials = 1;
    scene->mMaterials = new aiMaterial*[1];
    scene->mMaterials[0] = new aiMaterial();

    // 5. Exporter를 사용하여 FBX 저장
    Assimp::Exporter exporter;
    // "fbx" 혹은 "fbxa"(ASCII FBX) 사용 가능
    aiReturn res = exporter.Export(scene, "fbx", path);

    // Export 내부적으로 객체를 복사/사용 후 외부 생성본은 삭제해야함
    delete scene; 

    return (res == aiReturn_SUCCESS);
}

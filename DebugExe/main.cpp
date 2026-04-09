#include <iostream>
#include "../MeshSimplicationDLL/dllmain.h"

#pragma comment(lib, "MeshSimplicationDLL.lib") 

int main() 
{
    std::cout << "Starting Test with Implicit Linking..." << std::endl;

    std::cout << "-> Calling SetSimplicateLevel(5)..." << std::endl;
    SetSimplicateLevel(5);

    std::cout << "-> Calling LoadModel()..." << std::endl;
    // 실제 존재하는 모델 파일 경로로 변경하세요.
    LoadModel("A_001_Ceratosaurus_Walking_Anim.FBX"); 

    std::cout << "-> Calling Simplicate()..." << std::endl;
    Simplicate();

    std::cout << "Test completed successfully." << std::endl;

    char c;
    std::cin >> c;

    return 0;
}
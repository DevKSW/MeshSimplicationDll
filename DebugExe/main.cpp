#include <iostream>
#include "../MeshSimplicationDLL/dllmain.h"

#pragma comment(lib, "MeshSimplicationDLL.lib") 

#define MODEL_PATH_1 "A_001_Ceratosaurus_Walking_Anim.FBX"
#define MODEL_PATH_2 "model/bunny.obj"
#define MODEL_PATH_3 "A_002_Ceratosaurus_Walking2_Anim.FBX"
#define MODEL_PATH_4 "kratos.obj"
#define MODEL_PATH_5 "personal.obj"

int main() 
{
    std::cout << "Starting Test with Implicit Linking..." << std::endl;
    int simplicateLevel, simplicateRate;

    
    std::cout << "-> Enter the Simplicate Level... (Recomand : 5)" << std::endl;
    std::cin >> simplicateLevel;
    SetSimplicateLevel(simplicateLevel);


    std::cout << "-> Enter the Simplicate Rate... (1 is 1%)" << std::endl;
    std::cin >> simplicateRate;
    SetSimplicateRate(simplicateRate);

    std::cout << "Select Model : " << std::endl;
    std::cout << "\t 1 : " << MODEL_PATH_1 << std::endl;
    std::cout << "\t 2 : " << MODEL_PATH_2 << std::endl;
    std::cout << "\t 3 : " << MODEL_PATH_3 << std::endl;
    std::cout << "\t 4 : " << MODEL_PATH_4 << std::endl;
    std::cout << "\t 5 : " << MODEL_PATH_5 << std::endl;
    
    int modelIndex;
    std::cin >> modelIndex;

    std::cout << "-> Calling LoadModel()..." << std::endl;   

    switch (modelIndex)
    {
    case 1 :
    default:
        LoadModel(MODEL_PATH_1);
        break;        
    case 2 : 
        LoadModel(MODEL_PATH_2);
        break;
    case 3:
        LoadModel(MODEL_PATH_3);
        break;
    case 4:
        LoadModel(MODEL_PATH_4);
        break;
    case 5:
        LoadModel(MODEL_PATH_5);
        break;
    }
   

    std::cout << "-> Set Coordinate()..." << std::endl;
    SetCoordinate(1);

    std::cout << "-> Calling Simplicate()..." << std::endl;        
    
    Simplicate();

    std::cout << "Test completed successfully." << std::endl;

    char c;
    std::cin >> c;

    return 0;
}
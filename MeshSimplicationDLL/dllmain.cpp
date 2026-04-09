// dllmain.cpp : DLL 애플리케이션의 진입점을 정의합니다.
#include "pch.h"
#include "MeshSimplicator.h"

MeshSimplicator g_Simplicator;

extern "C" __declspec(dllexport) void SetSimplicateLevel(int level)
{
    g_Simplicator.SetSimplicateLevel(level);
}

extern "C" __declspec(dllexport) void LoadModel(const char* path) 
{    
    g_Simplicator.LoadModel(path);
}

extern "C" __declspec(dllexport) void Simplicate()
{
    g_Simplicator.Simplicate();
}

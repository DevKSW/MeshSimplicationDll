// dllmain.cpp : DLL 애플리케이션의 진입점을 정의합니다.
#include "pch.h"
#include "MeshSimplicator.h"
#include "dllmain.h"

MeshSimplicator g_Simplicator;

extern "C" __declspec(dllexport) void SetSimplicateLevel(int level)
{
    g_Simplicator.SetSimplicateLevel(level);
}

extern "C" __declspec(dllexport) void SetSimplicateRate(int rate)
{
    g_Simplicator.SetSimplicateRate(rate);
}


extern "C" __declspec(dllexport) void LoadModel(const char* path) 
{    
    g_Simplicator.LoadModel(path);
}

extern "C" __declspec(dllexport) void Simplicate()
{
    g_Simplicator.Simplicate();
}

extern "C" __declspec(dllexport) void  SetCoordinate(int flag)
{
    if (flag == 0)
        g_Simplicator.SetCoordinate(CoordinateFlag::Source);
    if (flag == 1)
        g_Simplicator.SetCoordinate(CoordinateFlag::Unreal);
    if (flag == 2)
        g_Simplicator.SetCoordinate(CoordinateFlag::Unity);
}

void Reset()
{     
    g_Simplicator.Reset();
}



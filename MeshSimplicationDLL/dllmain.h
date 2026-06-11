#pragma once

extern "C" __declspec(dllexport) void SetSimplicateLevel(int level);

extern "C" __declspec(dllexport) void SetSimplicateRate(int rate);

extern "C" __declspec(dllexport) void LoadModel(const char* path);

extern "C" __declspec(dllexport) void Simplicate();

extern "C" __declspec(dllexport) void SetCoordinate(int flag);

extern "C" __declspec(dllexport) void Reset();


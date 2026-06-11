#pragma once
#include <spdlog/spdlog.h>

class Logger {
public:   
    static void Init();
        
    static std::shared_ptr<spdlog::logger>& GetOutput();

private:
    static std::shared_ptr<spdlog::logger> m_FileLogger;
};

// 
#define LOG_INFO(...)  Logger::GetOutput()->info(__VA_ARGS__)
#define LOG_WARN(...) Logger::GetOutput()->warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::GetOutput()->error(__VA_ARGS__)
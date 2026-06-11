#include "pch.h"
#include "Logger.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <memory>
#include <string>

std::shared_ptr<spdlog::logger> Logger::m_FileLogger;

void Logger::Init() {
    if (m_FileLogger) return; // 이미 초기화되었다면 무시

    // 1. 현재 시간 구하기 (파일명용: YYYY-MM-DD_HH-MM-SS)
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::tm bt;
    localtime_s(&bt, &in_time_t); // Windows 안전 버전

    std::ostringstream oss;
    oss << "logs/log_" << std::put_time(&bt, "%Y-%m-%d_%H-%M-%S") << ".txt";
    std::string filename = oss.str();

    // 2. 싱크 생성 (콘솔 싱크는 콘솔 창이 없는 앱에서 DLL 호출 시 크래시 유발 가능성이 있으므로 우선 제외)
    // auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true);

    // 3. 파일 싱크만 등록
    m_FileLogger = std::make_shared<spdlog::logger>("DLL_LOGGER", spdlog::sinks_init_list{ file_sink });

    // 로그 패턴 설정 (시간 [레벨] 메시지)
    m_FileLogger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    m_FileLogger->set_level(spdlog::level::trace);

    // 에러 발생 시 파일에 즉시 쓰기 대신, 모든 로그마다 즉시 파일에 쓰기(디버깅용)
    m_FileLogger->flush_on(spdlog::level::trace);
    spdlog::register_logger(m_FileLogger);
}

std::shared_ptr<spdlog::logger>& Logger::GetOutput() {
    if (!m_FileLogger)
        Init();
    return m_FileLogger;
}
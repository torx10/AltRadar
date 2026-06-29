#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace RadarData {

class RadarLog {
public:
    static RadarLog& Instance() {
        static RadarLog log;
        return log;
    }

    void Init(const std::filesystem::path& pluginDir) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_path = pluginDir / "logs" / "altradar.log";
        std::error_code ec;
        std::filesystem::create_directories(m_path.parent_path(), ec);
        m_stream.open(m_path, std::ios::out | std::ios::app);
        if (m_stream.is_open()) {
            m_stream << "\n--- Alt Radar session start ---\n";
            m_stream.flush();
        }
    }

    void Shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stream.is_open()) {
            m_stream << "--- Alt Radar session end ---\n";
            m_stream.flush();
            m_stream.close();
        }
    }

    void Write(const char* level, const std::string& msg) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_stream.is_open()) return;
        const auto now = std::chrono::system_clock::now();
        const auto t = std::chrono::system_clock::to_time_t(now);
        char timeBuf[32]{};
        tm localTm{};
        localtime_s(&localTm, &t);
        strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &localTm);
        m_stream << '[' << timeBuf << "] [" << level << "] " << msg << '\n';
        m_stream.flush();
    }

    void Info(const std::string& msg) { Write("INFO", msg); }
    void Warn(const std::string& msg) { Write("WARN", msg); }
    void Error(const std::string& msg) { Write("ERROR", msg); }
    void Debug(const std::string& msg) { Write("DEBUG", msg); }

    std::filesystem::path Path() const { return m_path; }

private:
    RadarLog() = default;
    std::mutex              m_mutex;
    std::filesystem::path   m_path;
    std::ofstream           m_stream;
};

} // namespace RadarData

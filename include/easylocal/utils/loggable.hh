#pragma once

#include <spdlog/spdlog.h>

#include <spdlog/fmt/ostr.h>

namespace EasyLocal {
  namespace Core {
    class Loggable
    {
    public:
      template <typename... Args> void logtrace(const char* fmt, const Args&... args)
      { if (_logger) _logger->trace(fmt, args...); }
      
      template <typename... Args> void logdebug(const char* fmt, const Args&... args)
      { if (_logger) _logger->debug(fmt, args...); }
      
      template <typename... Args> void loginfo(const char* fmt, const Args&... args)
      { if (_logger) _logger->info(fmt, args...); }

      template <typename... Args> void logwarn(const char* fmt, const Args&... args)
      { if (_logger) _logger->warn(fmt, args...); }

      template <typename... Args> void logerror(const char* fmt, const Args&... args)
      { if (_logger) _logger->error(fmt, args...); }

      template <typename... Args> void logcritical(const char* fmt, const Args&... args)
      { if (_logger) _logger->critical(fmt, args...); }

    protected:
      Loggable(std::shared_ptr<spdlog::logger> logger) : _logger(logger) {}
      Loggable() {}
      std::shared_ptr<spdlog::logger> _logger;
    };
  }
}

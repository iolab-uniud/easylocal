#pragma once

#include <spdlog/spdlog.h>

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

      template <typename... Args> void lognotice(const char* fmt, const Args&... args)
      { if (_logger) _logger->notice(fmt, args...); }

      template <typename... Args> void logwarn(const char* fmt, const Args&... args)
      { if (_logger) _logger->warn(fmt, args...); }

      template <typename... Args> void logerror(const char* fmt, const Args&... args)
      { if (_logger) _logger->error(fmt, args...); }

      template <typename... Args> void logcritical(const char* fmt, const Args&... args)
      { if (_logger) _logger->critical(fmt, args...); }

      template <typename... Args> void logalert(const char* fmt, const Args&... args)
      { if (_logger) _logger->alert(fmt, args...); }

      template <typename... Args> void logemerg(const char* fmt, const Args&... args)
      { if (_logger) _logger->emerg(fmt, args...); }    

    protected:
      Loggable(std::shared_ptr<spdlog::logger> logger) : _logger(logger) {}
      Loggable() {}
      std::shared_ptr<spdlog::logger> _logger;
    };
  }
}
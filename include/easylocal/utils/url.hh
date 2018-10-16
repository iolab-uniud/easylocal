#pragma once
#include <string>

namespace EasyLocal {
  namespace Core {
    std::string URLEncode(const std::string &sSrc);
    std::string URLDecode(const std::string &sSrc);
  }
}

#ifndef AURORA_PCH
#define AURORA_PCH

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>

#include <string>
#include <string_view>
#include <sstream>

#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>

#include <optional>
#include <limits>

#include <chrono>

//NOTE: useful vulkan types?


namespace Aurora {
  template<typename T>
  using Scope = std::unique_ptr<T>;
  template<typename T>
  using Ref = std::shared_ptr<T>;

  //FIX: taken from Auralius stream? idk if its good or if i can use "using" or smth like that...
  template<typename O, typename I>
  [[nodiscard]] O as(I && v) { return static_cast<O>(std::forward<I>(v)); }
}

#include "utilities.h"
#include "logger.h"

#endif

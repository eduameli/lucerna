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

// include base macros... logging & assert
// namespace Ref and Scoped using!

namespace Aurora {
  template<typename T>
  using Scope = std::unique_ptr<T>;
  
  template<typename T>
  using Ref = std::shared_ptr<T>;
}


#endif

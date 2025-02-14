#pragma once
#include <cstdint>
#include <string_view>

namespace Lucerna { namespace spirv {

struct ParseResult
{
  uint32_t set_count{ 0 };
};

void parse_binary(uint32_t* data, size_t size, std::string_view name_buffer, ParseResult* parse_result);
  
}} // namespace Lucerna - spirv

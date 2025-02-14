#pragma once
#include "lucerna_pch.h"

#include <spirv/1.2/spirv.h>
#include <volk.h>

namespace Lucerna { namespace spirv {

struct StructMember
{
  uint32_t id_index;
  uint32_t offseT;
  std::string_view name;
};

struct Id
{
  SpvOp op;
  uint32_t set;
  uint32_t binding;

  uint8_t width;
  uint8_t sign;

  uint32_t type_index;
  uint32_t count;

  SpvStorageClass storage_class;

  uint32_t value;
  
  std::string_view name;
  std::vector<StructMember> members;
};

struct ParseResult
{
  uint32_t set_count{ 0 };
  VkShaderStageFlags stage;
  std::vector<Id> ids;
};


void parse_binary(uint32_t* data, size_t size, std::string_view name_buffer, ParseResult* parse_result);
VkShaderStageFlags parse_execution_model(SpvExecutionModel model);

}} // namespace Lucerna - spirv

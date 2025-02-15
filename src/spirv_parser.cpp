#include "spirv_parser.h"
#include "la_asserts.h"
#include "spirv/1.2/spirv.h"
#include <sys/types.h>

namespace Lucerna { namespace spirv {

void parse_binary(uint32_t* data, size_t size, std::string_view name_buffer, ParseResult* parse_result)
{
  uint32_t spv_word_count = (uint32_t) (size / 4);
  uint32_t magic_number = data[0];

  LA_LOG_ASSERT(magic_number == SpvMagicNumber, "SPIR-V Binary doesnt begin with the correct magic number!");
  uint32_t id_bound = data[3];

  parse_result->ids.resize(id_bound);

  // parsed information

  for (int word_idx = 5; word_idx < id_bound;)
  {
    SpvOp op = (SpvOp) (data[word_idx] & 0xFF);
    uint16_t word_count = (uint16_t) (data [word_idx] >> 16);




    switch (op)
    {
      case (SpvOpEntryPoint):
      {
        SpvExecutionModel model = (SpvExecutionModel) data[word_idx + 1];
        parse_result->stage = parse_execution_model(model);
        break;
        
      }
      case (SpvOpDecorate):
      {
        uint32_t id_index = data[word_idx + 1];
        Id& id = parse_result->ids.at(id_index);

        SpvDecoration decoration = (SpvDecoration) data[word_idx + 2];

        switch (decoration)
        {
          case (SpvDecorationBinding):
          {
            id.binding = data[word_idx + 3];
            break;
          }
          case (SpvDecorationDescriptorSet):
          {
            id.set = data[word_idx+ 3];
            break;
          }
          default:
          {
            break;
          }
        }
        
        break;    
      }
      
      case (SpvOpTypeVector):
      {
        uint32_t id_index = data[word_idx + 1];
        Id& id = parse_result->ids.at(id_index);
        id.op = op;
        id.type_index = data[word_idx + 2];
        id.count = data[word_idx + 3];
        break;    
      }

      case (SpvOpTypeSampler):
      {
        uint32_t id_index = data[word_idx];
        Id& id = parse_result->ids.at(id_index);
        id.op = op;
        break;
      }

      case (SpvOpVariable):
      {
        uint32_t id_index = data[word_idx + 2];
        Id& id = parse_result->ids.at(id_index);
        id.op = op;
        id.type_index = data[word_idx + 1];
        id.storage_class = (SpvStorageClass) data[word_idx + 3];
        break;
      }
      default:
      {
        
      }
        
    }


    
    word_idx += word_count;
  }
  
}

VkShaderStageFlags parse_execution_model(SpvExecutionModel model)
{
  return (VkShaderStageFlags) model;
}
  
}} // namespace Lucerna - spirv

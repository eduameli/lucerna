#include "vk_scene.h"
#include "engine.h"
#include "vk_pipelines.h"
#include "ar_asserts.h"
#include "vk_initialisers.h"

namespace Aurora {


void GLTFMetallic_Roughness::build_pipelines(Engine* engine)
{
  VkDevice device = engine->m_Device.logical;

  VkShaderModule meshFragShader;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/meshes/mesh.frag.spv", device, &meshFragShader),
    "Error when building the triangle fragment shader module"
  );

  VkShaderModule meshVertShader;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/meshes/mesh.vert.spv", device, &meshVertShader),
    "Error when building the triangle vertex shader module"
  );

  VkPushConstantRange matrixRange{};
  matrixRange.offset = 0;
  matrixRange.size = sizeof(GPUDrawPushConstants);
  matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  
  DescriptorLayoutBuilder layoutBuilder;
  layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

  materialLayout = layoutBuilder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
 
  VkDescriptorSetLayout layouts[] = {
    engine->m_SceneDescriptorLayout,
    materialLayout,
  };

  VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
  mesh_layout_info.setLayoutCount = 2;
  mesh_layout_info.pSetLayouts = layouts;
  mesh_layout_info.pPushConstantRanges = &matrixRange;
  mesh_layout_info.pushConstantRangeCount = 1;

  VkPipelineLayout newLayout;
  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &mesh_layout_info, nullptr, &newLayout));

  opaquePipeline.layout = newLayout;
  transparentPipeline.layout = newLayout;

  PipelineBuilder pipelineBuilder;
  pipelineBuilder.set_shaders(meshVertShader, meshFragShader);
  pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
  //pipelineBuilder.set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE); // NOTE: backface culling?
  pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  pipelineBuilder.set_multisampling_none();
  pipelineBuilder.disable_blending();
  pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

  pipelineBuilder.set_color_attachment_format(engine->m_DrawImage.imageFormat);
  pipelineBuilder.set_depth_format(engine->m_DepthImage.imageFormat);

  pipelineBuilder.PipelineLayout = newLayout;

  opaquePipeline.pipeline = pipelineBuilder.build_pipeline(device);

  pipelineBuilder.enable_blending_additive();
  pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

  transparentPipeline.pipeline = pipelineBuilder.build_pipeline(device);

  vkDestroyShaderModule(device, meshFragShader, nullptr);
  vkDestroyShaderModule(device, meshVertShader, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
  MaterialInstance matData;
  matData.passType = pass;
  if (pass == MaterialPass::Transparent)
  {
    matData.pipeline = &transparentPipeline;
  }
  else
  {
    matData.pipeline = &opaquePipeline;
  }

  matData.materialSet = descriptorAllocator.allocate(device, materialLayout);

  writer.clear();
  writer.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  writer.write_image(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.write_image(2, resources.metalRoughImage.imageView, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.update_set(device, matData.materialSet);
  return matData;
}

void MeshNode::draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
  glm::mat4 nodeMatrix = topMatrix * worldTransform;
  for (auto& s : mesh->surfaces)
  {
    RenderObject def;
    def.indexCount = s.count;
    def.firstIndex = s.startIndex;
    def.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
    def.material = &s.material->data;
    def.bounds = s.bounds;
    def.transform = nodeMatrix;
    def.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;
    ctx.OpaqueSurfaces.push_back(def);
  }
  
  Node::draw(topMatrix, ctx);

}

} // namespace 

#pragma once
#include "vk_types.h"

namespace Lucerna {

class Renderer
{
  public:
    void init();
    void shutdown();

    void init_draw_sets();
    void shutdown_draw_sets();

    void draw(VkCommandBuffer cmd);
  public:
    DrawSet opaque_set{.name = "(opaque-set)"};
    DrawSet transparent_set{.name = "(transparent-set)"};
  private:
    void draw_editor(VkCommandBuffer cmd, VkImageView target);
    void draw_depth_prepass(VkCommandBuffer cmd);
    void draw_geometry(VkCommandBuffer cmd);
    void draw_shadow_pass(VkCommandBuffer cmd);

    void cull_draw_set(VkCommandBuffer cmd, DrawSet& draw_set);

  private:
    AllocatedImage m_DrawImage, m_DepthImage;
};


} // namespace Lucerna

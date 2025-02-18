#pragma once
#include "vk_types.h"

namespace Lucerna::Renderer {
    void init();
    void shutdown();

    void init_draw_sets();
    void shutdown_draw_sets();

    void draw(VkCommandBuffer cmd);
    void draw_editor(VkCommandBuffer cmd, VkImageView target);
    void draw_depth_prepass(VkCommandBuffer cmd);
    void draw_geometry(VkCommandBuffer cmd);
    void draw_shadow_pass(VkCommandBuffer cmd);

    void cull_draw_set(VkCommandBuffer cmd, DrawSet& draw_set);
} // namespace Lucerna::Renderer

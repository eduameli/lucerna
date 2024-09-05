#pragma once

#include "vk_types.h"

#include <glm/fwd.hpp>

class GLFWwindow;

namespace Aurora
{
  class Camera
  {
    public:
      void init();
      static inline glm::vec3 velocity{0.0f};
      static inline float speed = 1.0f;
      glm::vec3 position;
      static inline float pitch{0.0f};
      static inline float yaw{0.0f};
      static inline float s_LastX{400.0f};
      static inline float s_LastY{300.0f};
      static inline float s_Sens{0.05f};
      static inline bool capture_mouse = false;
      glm::mat4 get_view_matrix();
      glm::mat4 get_rotation_matrix();
      static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
      static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
      static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
      static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
      void update();
  };
} // namespace aurora

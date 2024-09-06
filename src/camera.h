#pragma once

#include <glm/fwd.hpp>
#include "vk_types.h"

class GLFWwindow;

namespace Aurora
{
  class Camera
  {
    public:
      void init();
      void update();
      glm::mat4 get_view_matrix();
      glm::mat4 get_rotation_matrix();
    private:
      static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
      static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
      static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
      static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    private:
      // NOTE: right now they are static for convenience, multiple cameras or windows would break this!
      // a proper input system that subscribes to the callbacks and forwards inputs to any specific camera would work better 
      static inline glm::vec3 s_Velocity{0.0f};
      static inline float s_Speed = 0.2f;
      static inline glm::vec3 s_Position {0.0f};
      static inline float s_Pitch{0.0f};
      static inline float s_Yaw{0.0f};
      static inline float s_LastMouseX{0.0f};
      static inline float s_LastMouseY{0.0f};
      static inline bool s_CursorCaptured{false};
  };
} // namespace aurora

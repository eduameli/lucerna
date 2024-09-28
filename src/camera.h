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
    public:
      static inline glm::vec3 s_Position{2.5, 2.5, 2.5};
    private:
      // NOTE: no input system multi window support 
      static inline glm::vec3 s_Velocity{0.0f};
      static inline float s_Speed{0.2f};
      static inline float s_Pitch{0.0f};
      static inline float s_Yaw{0.0f};
      static inline float s_LastMouseX{0.0f};
      static inline float s_LastMouseY{0.0f};
      static inline bool s_CursorCaptured{false};
  };
} // namespace aurora

#pragma once

#include <glm/fwd.hpp>
#include "vk_types.h"

class GLFWwindow;

namespace Lucerna
{
  class Camera
  {
    public:
      void init();
      void update();
      glm::mat4 get_view_matrix();
      glm::mat4 get_rotation_matrix();
    public:
      static inline glm::vec3 s_Position{0, 0, 0};
    private:
      // FIX: make a proper controller/input system 
      static inline glm::vec3 s_Velocity{0.0f};
      static inline float s_Speed{0.02f};
      static inline float s_Pitch{0.0f};
      static inline float s_Yaw{0.0f};
      static inline float s_LastMouseX{0.0f};
      static inline float s_LastMouseY{0.0f};
      static inline bool s_CursorCaptured{false};
  };
} // namespace Lucerna

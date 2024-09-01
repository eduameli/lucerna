#include "vk_types.h"
#include "GLFW/glfw3.h"
#include "glm/vec3.hpp"

namespace Aurora
{
  class Camera
  {
    public:
      void init();
      static inline glm::vec3 velocity{0.0f};
      glm::vec3 position;
      static inline float pitch{0.0f};
      static inline float yaw{0.0f};
      static inline float s_LastX{400.0f};
      static inline float s_LastY{300.0f};
      static inline float s_Sens{0.05f};
      glm::mat4 get_view_matrix();
      glm::mat4 get_rotation_matrix();
      static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
      static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
      void update();
  };
} // namespace aurora

#include "camera.h"

#include "window.h"

#include <GLFW/glfw3.h>


#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

namespace Lucerna
{
  void Camera::init()
  {
    // wasd movement
    
    GLFWwindow* win = Window::get();

    glfwSetKeyCallback(win, [](GLFWwindow* window, int key, int scancode, int action, int mods){
      ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    
      ImGuiIO& io = ImGui::GetIO();
      if (io.WantTextInput) return;

      if (action == GLFW_PRESS)
      {
        if (key == GLFW_KEY_W) {s_Velocity.z = -1;}
        if (key == GLFW_KEY_S) {s_Velocity.z = 1;}
        if (key == GLFW_KEY_A) {s_Velocity.x = -1;}
        if(key == GLFW_KEY_D) {s_Velocity.x = 1;}
      }
      
      if (action == GLFW_RELEASE)
      {
        if (key == GLFW_KEY_W) {s_Velocity.z = 0;}
        if (key == GLFW_KEY_S) {s_Velocity.z = 0;}
        if (key == GLFW_KEY_A) {s_Velocity.x = 0;}
        if(key == GLFW_KEY_D) {s_Velocity.x = 0;}
      }
    });
    
    // move camera
    glfwSetCursorPosCallback(win, [](GLFWwindow* window, double xpos, double ypos) {
      ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos); 
      
      float xOffset = xpos - s_LastMouseX;
      float yOffset = s_LastMouseY - ypos;
      
      s_LastMouseX = xpos;
      s_LastMouseY = ypos;
    
      if (s_CursorCaptured)
      {
        s_Yaw += xOffset / 200.0f;
        s_Pitch += yOffset / 200.0f;
      }
    });
    
    // left click to move camera
    glfwSetMouseButtonCallback(win, [](GLFWwindow* window, int button, int action, int mods) {
      ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
      
      if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
      {
        s_CursorCaptured = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      }
      else
      {
        s_CursorCaptured = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      }

    });
    
    // scroll to control speed
    glfwSetScrollCallback(win, [](GLFWwindow* window, double xoffset, double yoffset){
      ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);

      float v = s_Speed + yoffset*0.01;
      s_Speed = std::clamp(v, 0.0001f, 1.0f);
 
    });
  }

  void Camera::update()
  {
    glm::mat4 cameraRotation = get_rotation_matrix();
    s_Position += glm::vec3(cameraRotation * glm::vec4(s_Velocity * s_Speed, 0.0f));
  }

  glm::mat4 Camera::get_view_matrix()
  {
    glm::mat4 cameraTranslation = glm::translate(glm::mat4{1.0f}, s_Position);
    glm::mat4 cameraRotation = get_rotation_matrix();
    return glm::inverse(cameraTranslation * cameraRotation);
  }

  glm::mat4 Camera::get_rotation_matrix()
  {
    glm::quat pitchRotation = glm::angleAxis(s_Pitch, glm::vec3{1.0f, 0.0f, 0.0f});
    glm::quat yawRotation = glm::angleAxis(s_Yaw, glm::vec3{0.0f, -1.0f, 0.0f});
    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
  }

} // namespace Lucerna

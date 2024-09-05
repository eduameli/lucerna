#include "camera.h"
#include "window.h"
#include <GLFW/glfw3.h>

#include <glm/mat4x4.hpp>
#include <glm/gtx/quaternion.hpp>
#include <imgui.h>
#include "aurora_pch.h"
#include "logger.h"
namespace Aurora
{
  void Camera::init()
  {
    if (glfwRawMouseMotionSupported())
      glfwSetInputMode(Window::get_handle(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    glfwSetKeyCallback(Window::get_handle(), key_callback);
    // FIXME: this breaks imgui input
    glfwSetCursorPosCallback(Window::get_handle(), cursor_pos_callback);
    glfwSetMouseButtonCallback(Window::get_handle(), mouse_button_callback);
    glfwSetScrollCallback(Window::get_handle(), scroll_callback);
    //glfwSetInputMode(Window::get_handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  }

  void Camera::update()
  {
    glm::mat4 cameraRotation = get_rotation_matrix();
    position += glm::vec3(cameraRotation * glm::vec4(velocity * speed, 0.0f));
  }
  
  void Camera::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
  {
    if (action == GLFW_PRESS)
    {
      if (key == GLFW_KEY_W) {velocity.z = -1;}
      if (key == GLFW_KEY_S) {velocity.z = 1;}
      if (key == GLFW_KEY_A) {velocity.x = -1;}
      if(key == GLFW_KEY_D) {velocity.x = 1;}
    }
    
    if (action == GLFW_RELEASE)
    {
      if (key == GLFW_KEY_W) {velocity.z = 0;}
      if (key == GLFW_KEY_S) {velocity.z = 0;}
      if (key == GLFW_KEY_A) {velocity.x = 0;}
      if(key == GLFW_KEY_D) {velocity.x = 0;}

    }
  }

  void Camera::cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
  {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent((float) xpos, (float) ypos);
    
    if (!capture_mouse)
    {
      s_LastX = xpos;
      s_LastY = ypos;
      return;
    }

    float xOffset = xpos - s_LastX;
    float yOffset = s_LastY - ypos;

    s_LastX = xpos;
    s_LastY = ypos;

    //yOffset *= s_Sens;
    //xOffset *= s_Sens;

    yaw += xOffset / 200.0f;
    pitch += yOffset / 200.0f;
  
    
  }
  
  void Camera::mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
  {
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
      capture_mouse = true;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    else
    {
      capture_mouse = false;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    
  }

  void Camera::scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
  {
    float v = speed + yoffset*0.1;
    speed = std::clamp(v, 0.05f, 5.0f);
    AR_CORE_TRACE("speed {}", speed);
  }

  glm::mat4 Camera::get_view_matrix()
  {
    glm::mat4 cameraTranslation = glm::translate(glm::mat4{1.0f}, position);
    glm::mat4 cameraRotation = get_rotation_matrix();
    return glm::inverse(cameraTranslation * cameraRotation);
  }

  glm::mat4 Camera::get_rotation_matrix()
  {
    glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{1.0f, 0.0f, 0.0f});
    glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{0.0f, -1.0f, 0.0f});
    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
  }

} // namespace aurora

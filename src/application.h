#pragma once
#include "aurora_pch.h"
#include "window.h"
#include "device.h"
#include "engine.h"

#include <vulkan/vulkan.h>
 

namespace Aurora {
  class Application
  {
    public:
      Application();
      ~Application();
      void Run();
      inline bool ShouldClose() { return glfwWindowShouldClose(m_MainWindow.GetHandle()); }
    public:
      static constexpr int WIDTH = 640;
      static constexpr int HEIGHT = 480;
    private:
    private:
      Window m_MainWindow;
      Engine m_Engine; 
  };
}

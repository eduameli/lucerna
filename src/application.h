#pragma once

#include "engine.h"
#include "window.h"

// CLI for different settings -load settings file etc? VULKAN / OPENGL, working dir etc.. VERBOSE LEVEL
namespace Aurora {
  class Application
  {
    public:
      Application();
      ~Application();
      void run();
      static Application& Get() { return *s_Instance; }
    public:
      static constexpr int WIDTH = 1024;
      static constexpr int HEIGHT = 860;
    private:
      static Application* s_Instance;
      Engine m_Engine;
      Window m_Window;
  };
} // namespace aurora

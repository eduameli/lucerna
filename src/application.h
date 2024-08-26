#pragma once
#include "window.h"
#include "engine.h"
 
// CLI for different settings -load settings file etc? VULKAN / OPENGL, working dir etc.. VERBOSE LEVEL
namespace Aurora {
  class Application
  {
    public:
      Application();
      ~Application();
      void run();
      static Window& get_main_window() { return self->m_Window; }
    public:
      static constexpr int WIDTH = 1024;
      static constexpr int HEIGHT = 860;
    private:
      Engine m_Engine;
      Window m_Window;
      static Application* self;
  };
} // namespace aurora

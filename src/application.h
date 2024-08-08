#pragma once
#include "aurora_pch.h"
#include "window.h"
#include "engine.h"

#include <vulkan/vulkan.h>
 
//NOTE: this will manage high level abstractions 
// engine -> rendering
// user input -> move around
// imgui maybe=

namespace Aurora {
  class Application
  {
    public:
      static constexpr int WIDTH = 800;
      static constexpr int HEIGHT = 600;
    public:
      Application();
      ~Application();
      void run();
      static bool should_exit();
      static Window& get_main_window();
    private: 
      static Window s_MainWindow;
      Engine m_Engine;
  };

}

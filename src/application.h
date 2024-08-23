#pragma once
#include "aurora_pch.h"
#include "window.h"
#include "engine.h"

#include <vulkan/vulkan.h>
 
//NOTE: Application handles higher-level systems
// engine -> rendering
// user input -> move around camera
// imgui demo settings

namespace Aurora {
  class Application
  {
    public:
      static constexpr int WIDTH = 1024;
      static constexpr int HEIGHT = 860;
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

#pragma once

#include "engine.h"
#include "window.h"

namespace Aurora {
  class Application
  {
    public:
      Application();
      ~Application();
      void run();
    public:
      static inline struct Configuration
      {
        // startup
        std::string scene_path;

        // general
        glm::uvec2 internal_resolution;
        
        // window
        glm::uvec2 resolution;
      } config;
    private:
      Engine m_Engine;
      Window m_Window;
  };
} // namespace aurora

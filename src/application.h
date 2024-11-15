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
        std::string scene_path;
      } config;
    private:
      Engine m_Engine;
      Window m_Window;
  };
} // namespace aurora

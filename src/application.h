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
    private:
      Engine m_Engine;
      Window m_Window;
  };
} // namespace aurora

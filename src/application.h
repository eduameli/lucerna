#include "aurora_pch.h"
#include "window.h"

namespace Aurora {
  class Application
  {
    public:
      Application();
      ~Application();
      inline bool ShouldClose() { return m_MainWindow.ShouldClose(); }
      inline void PollEvents() { return m_MainWindow.PollEvents(); }
    public:
      static constexpr int WIDTH = 640;
      static constexpr int HEIGHT = 480;
    private:
      void Run();
    private:
      Window m_MainWindow;
    };
}

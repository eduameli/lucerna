#include "application.h"

namespace Aurora {

  //Window Application::s_Window;
  Application* Application::self = nullptr;

  Application::Application()
  {
    self = this;
    // read config files and save
    Window::Config config{1024, 680, "aurora"};
    m_Window.init(config);
  }

  Application::~Application()
  {
    // finish logging into a file
    m_Window.shutdown();
  }

  void Application::run()
  {
    // NOTE: configure engine m_Engine.init() / m_Engine.init(Config& config);
    m_Engine.init();
    m_Engine.run();
    m_Engine.shutdown();
  }

} // namespace aurora

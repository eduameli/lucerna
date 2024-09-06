#include "application.h"

namespace Aurora {

  Application::Application()
  {
    // read and set preferred configuration
    Window::init("aurora", 1280, 800);
  }

  Application::~Application()
  {
    // finish logging into a file
    Window::shutdown(); 
  }

  void Application::run()
  {
    // NOTE: configure engine startup from args or config.toml file 
    m_Engine.init();
    m_Engine.run();
    m_Engine.shutdown();
  }

} // namespace aurora

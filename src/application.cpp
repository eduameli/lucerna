#include "application.h"

namespace Aurora {

  Application::Application()
  {
    // read and set preferred configuration
    Window::init("aurora", 1024, 640);
  }

  Application::~Application()
  {
    // finish logging into a file
    Window::shutdown(); 
  }

  void Application::run()
  {
    // NOTE: configure engine m_Engine.init() / m_Engine.init(Config& config);
    m_Engine.init();
    m_Engine.run();
    m_Engine.shutdown();
  }

} // namespace aurora

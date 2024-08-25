#include "aurora_pch.h"
#include "application.h"

#include <thread>
#include <chrono>

namespace Aurora {

Window Application::s_MainWindow{Application::WIDTH, Application::HEIGHT, "aurora"};

Application::Application()
{
}

Application::~Application()
{
}

void Application::run()
{
  // NOTE: configure engine 
  //m_Engine.init() / m_Engine.init(Config& config);
  m_Engine.init();
  m_Engine.run();
  m_Engine.shutdown();
}

bool Application::should_exit()
{
  return s_MainWindow.should_close();
}

Window& Application::get_main_window()
{
  return s_MainWindow;
}

}

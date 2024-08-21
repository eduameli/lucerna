#include "aurora_pch.h"
#include "application.h"
namespace Aurora {

Window Application::s_MainWindow{Application::WIDTH, Application::HEIGHT, "Aurora"};

Application::Application()
{}

Application::~Application()
{}

void Application::run()
{
  auto start = std::chrono::system_clock::now();
  glfwPollEvents();
  m_Engine.draw();
  auto end = std::chrono::system_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  m_Engine.stats.frametime = elapsed.count() / 1000.0f;
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

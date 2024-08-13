#include "aurora_pch.h"
#include "application.h"
namespace Aurora {

Window Application::s_MainWindow{Application::WIDTH, Application::HEIGHT, "Aurora"};

Application::Application()
//  : m_MainWindow{WIDTH, HEIGHT, "Aurora"}, m_Engine{m_MainWindow}
{
  //m_Window = {WIDTH, HEIGHT, "Aurora"};

}

Application::~Application()
{
}

void Application::run()
{
  glfwPollEvents();
  m_Engine.draw();
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

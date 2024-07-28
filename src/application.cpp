#include "aurora_pch.h"
#include "application.h"
namespace Aurora {

Application::Application()
  : m_MainWindow{WIDTH, HEIGHT, "Aurora"}, m_Engine{m_MainWindow}
{
  //InitialiseVulkan();
}

Application::~Application()
{
}

void Application::Run()
{
  glfwPollEvents();
  m_Engine.Draw();
}
}

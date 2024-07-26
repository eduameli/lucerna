#include "aurora_pch.h"
#include "application.h"
namespace Aurora {

Application::Application()
  : m_MainWindow{WIDTH, HEIGHT, "Aurora"}, m_Device(m_MainWindow), m_Engine{}
{
  //InitialiseVulkan();
}

Application::~Application()
{
}

void Application::Run()
{
  glfwPollEvents();
}
}

#include "aurora_pch.h"
#include "application.h"

namespace Aurora {

Application::Application()
  : m_MainWindow{WIDTH, HEIGHT, "Aurora"}, m_Device(m_MainWindow)
{
  //InitialiseVulkan();
}

Application::~Application()
{
}

void Application::Run()
{
  glfwPollEvents();
  //AR_CORE_TRACE("new frame!");
}
}

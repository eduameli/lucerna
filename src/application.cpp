#include "aurora_pch.h"
#include "application.h"

namespace Aurora {

Application::Application()
  : m_MainWindow{WIDTH, HEIGHT, "Aurora"}
{
  //InitialiseVulkan();
}

Application::~Application()
{
}

void Application::Run()
{
  //AR_CORE_TRACE("new frame!");
}
}

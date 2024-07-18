#include "aurora_pch.h"
#include "application.h"

namespace Aurora {

Application::Application()
  : m_MainWindow{WIDTH, HEIGHT, "Aurora"}
{
  AR_CORE_INFO("Constructing Application", 640, 480);
}

Application::~Application()
{
  AR_CORE_INFO("Destroying Application");
}

void Application::Run()
{
}

}

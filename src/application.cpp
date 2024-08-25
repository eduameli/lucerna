#include "aurora_pch.h"
#include "application.h"

#include <thread>
#include <chrono>

namespace Aurora {

Window Application::s_MainWindow{Application::WIDTH, Application::HEIGHT, "aurora"};

Application::Application()
{}

Application::~Application()
{}

void Application::run()
{
  // other thread?
  while (!should_exit())
  {
    auto start = std::chrono::system_clock::now();
    glfwPollEvents();

    if (Engine::get().stop_rendering)
    {
      std::cout << "SLeeping!!" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }
    
    if(Engine::get().resize_requested)
      Engine::get().resize_swapchain();

    m_Engine.draw();
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    m_Engine.stats.frametime = elapsed.count() / 1000.0f; 
  }
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

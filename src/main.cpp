#include "aurora_pch.h"
#include "application.h"
int main(int argc, char* argv[])
{
  Aurora::Log::Init();
  
  Aurora::Application app;
  while (!app.ShouldClose())
  {
    app.PollEvents();
    app.Run();
  }
  
  // something like this!
  //Aurora::Application::cleanup();
  // vkh (vulkan helper!) 

  
}

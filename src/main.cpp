#include "aurora_pch.h"
#include "application.h"
int main(int argc, char* argv[])
{
  Aurora::Log::Init();
  //AR_CORE_FATAL("STARTING"); 
  Aurora::Application app;
  std::cout << "should exit?" << std::endl;
  while(!app.should_exit())
  {
    app.run();
  }
  //while (!app.should_exit())
  //{
  //  app.run();
  //}
}

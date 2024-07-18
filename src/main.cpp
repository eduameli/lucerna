#include "aurora_pch.h"
#include "application.h"
int main(int argc, char* argv[])
{
  Aurora::Log::Init();
  
  AR_CORE_INFO("TESTING ASSERT");
  AR_ASSERT(1 == 2);

  Aurora::Application app;
  while (!app.ShouldClose())
  {
    app.PollEvents();
  }
  
}

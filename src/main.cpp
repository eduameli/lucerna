#include "aurora_pch.h"
#include "application.h"
int main(int argc, char* argv[])
{
  Aurora::Log::Init();

  Aurora::Application app;
  app.run();
}

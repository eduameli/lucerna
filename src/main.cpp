#include "aurora_pch.h"
#include "application.h"

int main(int argc, char* argv[])
{
  using namespace Aurora;
  Logger::init();

  Aurora::Application app;
  app.run();
}


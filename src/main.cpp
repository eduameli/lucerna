#include "application.h"
#include "logger.h"

int main(int argc, char* argv[])
{
  Aurora::Logger::init();

  Aurora::Application app;
  app.run();
}


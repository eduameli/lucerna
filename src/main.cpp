#include "logger.h"
#include "application.h"

int main(int argc, char* argv[])
{
  Aurora::Logger::info("starting lucerna");
  
  Aurora::Application app;
  app.run();

  Aurora::Logger::info("goodbye world :(");
}


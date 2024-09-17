#include "logger.h"
#include "application.h"
int main(int argc, char* argv[])
{
  Aurora::Logger::init();

  Aurora::Application app;
  app.run();
  
  AR_CORE_INFO("goodbye world :(");
}


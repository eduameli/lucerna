#include "logger.h"
#include "application.h"

int main(int argc, char* argv[])
{
  LA_LOG_INFO("Starting Lucerna...");

  Aurora::Application app;
  app.run();
  
  LA_LOG_INFO("goodbye world :(");
}


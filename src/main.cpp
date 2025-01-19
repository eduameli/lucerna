#include "logger.h"
#include "application.h"

int main(int argc, char* argv[])
{
  LA_LOG_INFO("Starting Lucerna...");

  Lucerna::Application app;
  app.run();
  
  LA_LOG_INFO("goodbye world :(");
}


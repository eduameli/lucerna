#include "application.h"
#include "aurora_pch.h"
#include "toml++/toml.hpp"

namespace Aurora {

  Application::Application()
  {
    // read and set preferred configuration
    Window::init("aurora", 1280, 800);

    if (std::filesystem::exists("configuration.toml"))
    {
      AR_CORE_INFO("Loading configuration.toml file...");
      auto toml = toml::parse_file("configuration.toml");

      config.scene_path = toml["startup"]["default_scene"].value_or("");
    }
    else
    {
      AR_CORE_TRACE("Doesnt exist...");
    }
  }

  Application::~Application()
  {
    // finish logging into a file
    Window::shutdown(); 
  }

  void Application::run()
  {
    // NOTE: configure engine startup from args or config.toml file 
    m_Engine.init();
    m_Engine.run();
    m_Engine.shutdown();
  }

} // namespace aurora

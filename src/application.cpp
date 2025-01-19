#include "application.h"
#include "lucerna_pch.h"
#include "la_asserts.h"

#include "toml++/impl/forward_declarations.hpp"
#include "toml++/impl/node.hpp"
#include "toml++/toml.hpp"

namespace Lucerna {

  Application::Application()
  {

    toml::parse_result result = toml::parse_file("config/configuration.toml");
    if (!result.succeeded())
    {
      // AR_CORE_ERROR("Error loading configuration file!");
      return;
    }
        auto toml = result.table();
    // AR_CORE_TRACE("Loaded configuration.toml");

    config.scene_path = toml["startup"]["scene_path"].value_or("assets/structure.glb");
    
    config.resolution = glm::vec2{
      toml["window"]["resolution"][0].value_or(1280),
      toml["window"]["resolution"][1].value_or(800)
    };

    
    config.internal_resolution = glm::vec2{
      toml["general"]["internal_resolution"][0].value_or(config.resolution.x),
      toml["general"]["internal_resolution"][1].value_or(config.resolution.y)
    };
    
    Window::init("lucerna-dev", config.resolution.x, config.resolution.y);
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

} // namespace Lucerna

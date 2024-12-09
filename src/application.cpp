#include "application.h"
#include "aurora_pch.h"
#include "ar_asserts.h"
#include "toml++/impl/forward_declarations.hpp"
#include "toml++/impl/node.hpp"
#include "toml++/toml.hpp"

namespace Aurora {

  Application::Application()
  {
    // read and set preferred configuration
    Window::init("Lucerna", 1280, 800);

    // hard coded defaults!
    config =
    {
      .scene_path = "assets/structure.glb"
    };

    toml::parse_result result = toml::parse_file("config/configuration.toml");

    if (!result.succeeded())
    {
      AR_CORE_ERROR("Error loading configuration file!");
      return;
    }
    
    auto toml = result.table();
    AR_CORE_TRACE("Loaded configuration.toml");

    auto update_config = [] <typename T> (T* field, toml::node_view<toml::node> node){
      if (node.is_value())
      {
        *field = node.ref<T>(); // WARN: dangerous if the types are mismatched will crash the program!
      }
    };

    update_config(&config.scene_path, toml["startup"]["scene_path"]);
    
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

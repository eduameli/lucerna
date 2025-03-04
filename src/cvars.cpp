#include "cvars.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "logger.h"
#include "imgui.h"

namespace Lucerna {

enum class CVarType : uint8_t
{
	INT,
	FLOAT,
  STRING, // NOTE: not implemented - useful for file paths!
};

struct CVarParameter
{
  friend class CVarSystemImpl;
  uint32_t arrayIndex;
  CVarType type;
  CVarFlags flags;
  std::string name;
  std::string description;
};

template<typename T>
struct CVarStorage
{
  T initial;
  T current;
  CVarParameter* parameter;
};

template<typename T>
struct CVarArray
{
  CVarStorage<T>* cvars;
  uint32_t last{ 0 };

  CVarArray(size_t size)
  {
    cvars = new CVarStorage<T>[size]();
  }
  ~CVarArray()
  {
    delete[] cvars;
  }

  T get_current(uint32_t index)
  {
    return cvars[index].current;
  }

  T* get_current_ptr(uint32_t index)
  {
    return &cvars[index].current;
  }

  void set_current(const T& value, int32_t index)
  {
    cvars[index].current = value;
  }

  int add(const T& value, CVarParameter* param)
  {
    int index = last;
    cvars[index].current = value;
    cvars[index].initial = value;
    cvars[index].parameter = param;

    param->arrayIndex = index;
    last++;
    return index;
  }

  int add(const T& initial, const T& current, CVarParameter* param)
  {
    int index = last;
    cvars[index].current = current;
    cvars[index].initial = initial;
    cvars[index].parameter = param;

    param->arrayIndex = index;
    last++;
    return index;
  }

};

class CVarSystemImpl : public CVarSystem
{
public:
  constexpr static int MAX_INT_CVARS = 100;
  CVarArray<int32_t> intCVars2{ MAX_INT_CVARS }; //FIX: why 2? :sob:

  constexpr static int MAX_FLOAT_CVARS = 100;
  CVarArray<float> floatCVars{ MAX_FLOAT_CVARS };

  template<typename T>
  CVarArray<T>* get_cvar_array();



  CVarParameter* get_cvar(StringUtils::StringHash hash) override final;

  CVarParameter* create_float_cvar(const char* name, const char* description, float initial, float current) override final;
  CVarParameter* create_int_cvar(const char* name, const char* description, int initial, int current) override final;
  
  float* get_float_cvar(StringUtils::StringHash hash) override final;
  void set_float_cvar(StringUtils::StringHash hash, float value) override final;

  int* get_int_cvar(StringUtils::StringHash hash) override final;
  void set_int_cvar(StringUtils::StringHash has, int value) override final;
  
  void draw_editor() override final;

  template<typename T>
  T* get_cvar_current(uint32_t hash)
  {
    CVarParameter* param = get_cvar(hash);
    if (!param) return nullptr;

    return get_cvar_array<T>()->get_current_ptr(param->arrayIndex);
  }

  template<typename T>
  void set_cvar_current(uint32_t hash, const T& value)
  {
    CVarParameter* cvar = get_cvar(hash);
    if (cvar)
    {
      get_cvar_array<T>()->set_current(value, cvar->arrayIndex);
    }
  }

  static CVarSystemImpl* get()
  {
    return static_cast<CVarSystemImpl*>(CVarSystem::get());
  }
  
public:
private:
  CVarParameter* init_cvar(const char* name, const char* description);
  std::unordered_map<uint32_t, CVarParameter> savedCVars;
  std::vector<CVarParameter*> searchResults;
};



template<>
CVarArray<int32_t>* CVarSystemImpl::get_cvar_array()
{
  return &intCVars2;
}

template<>
CVarArray<float>* CVarSystemImpl::get_cvar_array()
{
  return &floatCVars;
}

CVarParameter* CVarSystemImpl::init_cvar(const char* name, const char* description)
{
  if (get_cvar(name)) return nullptr;

  uint32_t hash = StringUtils::StringHash{ name };
  savedCVars[hash] = CVarParameter{
    .name = name,
    .description = description
  };

  return &savedCVars[hash];
}

CVarParameter* CVarSystemImpl::create_float_cvar(const char* name, const char* description, float initial, float current)
{
  CVarParameter* param = init_cvar(name, description);
  if (!param) return nullptr;
  
  param->type = CVarType::FLOAT;

  get_cvar_array<float>()->add(initial, current, param);
  return param;
}

CVarParameter* CVarSystemImpl::create_int_cvar(const char* name, const char* description, int initial, int current)
{
  CVarParameter* param = init_cvar(name, description);
  if (!param) return nullptr;
  
  param->type = CVarType::INT;

  get_cvar_array<int>()->add(initial, current, param);
  return param;
}

CVarParameter* CVarSystemImpl::get_cvar(StringUtils::StringHash hash)
{
  auto it = savedCVars.find(hash);
  if (it != savedCVars.end())
  {
    return &(*it).second;
  }

  return nullptr;
}

float* CVarSystemImpl::get_float_cvar(StringUtils::StringHash hash)
{
  return get_cvar_current<float>(hash);
}

void CVarSystemImpl::set_float_cvar(StringUtils::StringHash hash, float value)
{
  set_cvar_current<float>(hash, value);
}

int* CVarSystemImpl::get_int_cvar(StringUtils::StringHash hash)
{
  return get_cvar_current<int>(hash);
}

void CVarSystemImpl::set_int_cvar(StringUtils::StringHash hash, int value)
{
  set_cvar_current<int>(hash, value);
}

void CVarSystemImpl::draw_editor()
{
  ImGui::Begin("Console Variables");

  static char searchText[64] = "";
  ImGui::InputText("filter", searchText, 64);
	
  static bool showAdvanced = false;
	ImGui::Checkbox("show advanced", &showAdvanced);
	
  ImGui::Separator(); 
  
  searchResults.clear();

  auto addToSearchResults = [&](auto param)
  {
    bool isHidden = ((uint32_t)param->flags & (uint32_t)CVarFlags::Hidden);
    bool isAdvanced = ((uint32_t)param->flags & (uint32_t) CVarFlags::Advanced);

    if (!isHidden)
    {
      if(!(!showAdvanced & isAdvanced) && param->name.find(searchText) != std::string::npos)
      {
        searchResults.push_back(param);
      }
    }
  };

  for (int i = 0; i < get_cvar_array<int32_t>()->last; i++)
  {
    addToSearchResults(get_cvar_array<int32_t>()->cvars[i].parameter);
  }

  for (int i = 0; i < get_cvar_array<float>()->last; i++)
  {
    addToSearchResults(get_cvar_array<float>()->cvars[i].parameter);
  }
  

  std::sort(searchResults.begin(), searchResults.end(), [](CVarParameter* A, CVarParameter* B)
  {
    return A->name < B->name;
	});

  for (auto p : searchResults)
  {
    ImGui::Text("%s", p->name.c_str());
    if (ImGui::IsItemHovered() && p->description.size() != 0)
	  {
		  ImGui::SetTooltip("%s", p->description.c_str());
	  }

    switch (p->type)
    {
      case CVarType::INT:
      {
        ImGui::PushID(p->name.c_str());
        bool isCheckbox = ((uint32_t) p->flags & (uint32_t) CVarFlags::EditCheckbox);
        if (isCheckbox)
        {
          bool value = get_cvar_array<int32_t>()->get_current(p->arrayIndex) != 0;
          if (ImGui::Checkbox("", &value))
          {
            get_cvar_array<int32_t>()->set_current(value ? 1 : 0, p->arrayIndex);
          }
        }
        else
        {
          ImGui::InputInt("", get_cvar_array<int32_t>()->get_current_ptr(p->arrayIndex));
        }
        ImGui::PopID();
        break;
      }
      case CVarType::FLOAT:
        ImGui::PushID(p->name.c_str());
				ImGui::InputFloat("", get_cvar_array<float>()->get_current_ptr(p->arrayIndex), 0.0f, 0.0f, "%.5f");
				ImGui::PopID();
        break;
      default:
        LA_LOG_WARN("Attempted to display unregistered CVar type!");
    }
  }

  // filter - (able to edit all of them / for non editable info just put it in render settings)

  ImGui::End();
}

CVarSystem* CVarSystem::get()
{
  static CVarSystemImpl cvarSys{};
  return &cvarSys;
}

template<typename T>
T get_cvar_current_by_index(int32_t index)
{
  return CVarSystemImpl::get()->get_cvar_array<T>()->get_current(index);
}

template<typename T>
void set_cvar_current_by_index(int32_t index, const T& data)
{
  CVarSystemImpl::get()->get_cvar_array<T>()->set_current(data, index);
}

AutoCVar_Float::AutoCVar_Float(const char* name, const char* description, float initial, CVarFlags flags)
{
  CVarParameter* cvar = CVarSystem::get()->create_float_cvar(name, description, initial, initial);
  cvar->flags = flags;
  index = cvar->arrayIndex;
}

float AutoCVar_Float::get()
{
  return get_cvar_current_by_index<CVarType>(index);
}

float* AutoCVar_Float::get_ptr()
{
  return CVarSystemImpl::get()->get_cvar_array<float>()->get_current_ptr(index);
}

void AutoCVar_Float::set(float f)
{
  set_cvar_current_by_index<CVarType>(f, index);
}


AutoCVar_Int::AutoCVar_Int(const char* name, const char* description, int32_t initial, CVarFlags flags)
{
  CVarParameter* cvar = CVarSystem::get()->create_int_cvar(name, description, initial, initial);
  cvar->flags = flags;
  index = cvar->arrayIndex;
}

int32_t AutoCVar_Int::get()
{
  return get_cvar_current_by_index<CVarType>(index);
}

int32_t* AutoCVar_Int::get_ptr()
{
  return CVarSystemImpl::get()->get_cvar_array<int32_t>()->get_current_ptr(index);
}

void AutoCVar_Int::set(int32_t f)
{
  set_cvar_current_by_index<CVarType>(f, index);
}

void AutoCVar_Int::toggle()
{
  bool enabled = get() != 0;
  set(enabled ? 0 : 1);
}


} // namespace Lucerna

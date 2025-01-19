#pragma once
#include <cstdint>
#include <string_view>

namespace StringUtils {
	constexpr uint32_t fnv1a_32(char const* s, std::size_t count)
	{
		return ((count ? fnv1a_32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u;
	}

	constexpr size_t const_strlen(const char* s)
	{
		size_t size = 0;
		while (s[size]) { size++; };
		return size;
	}

	struct StringHash
	{
		uint32_t computedHash;
		constexpr StringHash(uint32_t hash) noexcept : computedHash(hash) {}
		constexpr StringHash(const char* s) noexcept : computedHash(0)
		{
			computedHash = fnv1a_32(s, const_strlen(s));
		}
		constexpr StringHash(const char* s, std::size_t count)noexcept : computedHash(0)
		{
			computedHash = fnv1a_32(s, count);
		}
		constexpr StringHash(std::string_view s)noexcept : computedHash(0)
		{
			computedHash = fnv1a_32(s.data(), s.size());
		}
		StringHash(const StringHash& other) = default;
		constexpr operator uint32_t()noexcept { return computedHash; }
	};
}

namespace Lucerna {

enum class CVarFlags : uint32_t
{
	None = 0,
	Hidden = 1 << 1,
	Advanced = 1 << 2,

	EditCheckbox = 1 << 8,
	EditFloatDrag = 1 << 9,
};

//inline CVarFlags operator|(CVarFlags lhs, CVarFlags rhs) {
//    return static_cast<CVarFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
//}



class CVarParameter;

class CVarSystem
{
  public:
    static CVarSystem* get();
    virtual CVarParameter* get_cvar(StringUtils::StringHash hash) = 0;
     
    virtual float* get_float_cvar(StringUtils::StringHash hash) = 0;
    virtual int32_t* get_int_cvar(StringUtils::StringHash hash) = 0;

    virtual void set_float_cvar(StringUtils::StringHash hash, float value) = 0;
    virtual void set_int_cvar(StringUtils::StringHash has, int32_t value) = 0;

    virtual CVarParameter* create_float_cvar(const char* name, const char* description, float initial, float current) = 0;
    virtual CVarParameter* create_int_cvar(const char* name, const char* description, int initial, int current) = 0;  
  
    virtual void draw_editor() = 0;
  
  public:
  private:
  private:
};


template<typename T>
struct AutoCVar
{
protected:
  int index;
  using CVarType = T;
};

struct AutoCVar_Float : AutoCVar<float>
{
  AutoCVar_Float(const char* name, const char* description, float initial, CVarFlags flags = CVarFlags::None);
  float get();
  float* get_ptr();
  void set(float value);
};

struct AutoCVar_Int : AutoCVar<int32_t>
{
	AutoCVar_Int(const char* name, const char* description, int32_t initial, CVarFlags flags = CVarFlags::None);
	int32_t get();
	int32_t* get_ptr();
	void set(int32_t val);

	void toggle();
};


}

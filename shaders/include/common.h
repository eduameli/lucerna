#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus

#include "volk.h"

#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#define uint32_ar uint32_t
#define float_ar float
#define bool_ar bool
#define vec2_ar glm::vec2
#define vec3_ar glm::vec3
#define vec4_ar glm::vec4
#define mat4_ar glm::mat4x4

#define buffer_ar(name) VkDeviceAddress

#else

#define uint32_ar uint
#define float_ar float
#define bool_ar bool
#define vec2_ar vec2
#define vec3_ar vec3
#define vec4_ar vec4
#define mat4_ar mat4

#define buffer_ar(name) name 

#define PI 3.14159

#endif // __CPLUSPLUS
#endif // COMMON_H

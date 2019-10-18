#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inTang;
layout(location = 2) in vec3 inNorm;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec2 outUV;

layout(binding = 0) uniform UBO {
    mat4 projectionMatrix;
    mat4 worldMatrix;
    mat4 viewMatrix;
} ubo;

void main() {
    outUV = inUV;
    gl_Position = ubo.projectionMatrix * ubo.viewMatrix * ubo.worldMatrix * vec4(inPos.xyz, 1.0);
}

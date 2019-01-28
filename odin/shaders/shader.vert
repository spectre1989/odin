#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_colour;

layout(location = 0) out vec3 out_colour;

out gl_PerVertex 
{
    vec4 gl_Position;
};

layout(set = 0, binding = 0) uniform UBO
{
	mat4 model_view_proj;
} ubo;

void main() 
{
    gl_Position = ubo.model_view_proj * vec4(in_position, 1.0);
    out_colour = in_colour;
}
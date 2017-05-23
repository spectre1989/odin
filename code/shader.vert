#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform Per_Instance_UBO
{
	mat4 matrix;
	vec3 colour;
} per_instance_ubo;

layout(location = 0) in vec2 in_position;

layout(location = 0) out vec3 out_colour;

out gl_PerVertex 
{
    vec4 gl_Position;
};

void main() 
{
    gl_Position = per_instance_vbo.matrix * vec4(in_position, 0.0, 1.0);
    out_colour = per_instance_vbo.colour;
}
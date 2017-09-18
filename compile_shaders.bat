%VULKAN_SDK%\Bin\glslangValidator.exe -V code\shader.vert
%VULKAN_SDK%\Bin\glslangValidator.exe -V code\shader.frag
mkdir build\data
move *.spv build\data\
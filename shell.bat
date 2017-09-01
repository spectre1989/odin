@echo off

if not defined VULKAN_SDK (
	echo VULKAN_SDK environment variable not found
) else ( 
	set INCLUDE="%VULKAN_SDK%\Include;%INCLUDE%" 
)

if defined VS150COMNTOOLS (
	echo Visual Studio 2017 detected
	"%VS150COMNTOOLS%..\..\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
) else (
	if defined VS140COMNTOOLS (
		echo Visual Studio 2015 detected
		"%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" x64
	) else (
		echo couldn't find Visual Studio installation, VSXX0COMNTOOLS environment variable not found
	)
)

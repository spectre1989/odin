@echo off

if not defined VULKAN_SDK (
	echo VULKAN_SDK environment variable not found
) else ( 
	set INCLUDE=%VULKAN_SDK%\Include;%INCLUDE%
)

rem doesn't seem to be a vs150comntools for 2017 ugh
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" (
	echo Visual Studio 2017 detected
	"C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
) else (
	if defined VS140COMNTOOLS (
		echo Visual Studio 2015 detected
		"%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" x64
	) else (
		echo couldn't find Visual Studio installation, VSXX0COMNTOOLS environment variable not found
	)
)

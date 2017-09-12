@echo off

if not defined VULKAN_SDK (
	echo VULKAN_SDK environment variable not found
) else ( 
	set INCLUDE=%VULKAN_SDK%\Include;%INCLUDE%
)

rem doesn't seem to be a vs150comntools for 2017 ugh
set VS15=
set KEY_NAME="HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\SxS\VS7"
set VALUE_NAME=15.0
FOR /F "usebackq skip=2 tokens=1-2*" %%A IN (`REG QUERY %KEY_NAME% /v %VALUE_NAME% 2^>nul`) DO (
    set VS15=%%C
)

if defined VS15 (
	if exist "%VS15%VC\Auxiliary\Build\vcvarsall.bat" (
		echo Visual Studio 2017 detected
		"%VS15%VC\Auxiliary\Build\vcvarsall.bat" x64
		goto end
	)
)

if defined VS140COMNTOOLS (
	if exist "%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" (
		echo Visual Studio 2015 detected
		"%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" x64
		goto end
	)
)

echo Couldn't find Visual Studio installation

:end

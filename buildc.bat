mkdir build
cd build

:: nologo - suppress startup banner
:: MT - use multi threaded static linked c runtime library
:: Z7 - put debug information in obj instead of pdb
:: Gm - minimal rebuild
:: GR - RTTI
:: EH - exceptions
:: Od - disable optimisations
:: Oi - use intrinsics
:: FC - full path of source code file in diagnostics
:: W4 - warning level 4
:: WX - treat warnings as errors
:: OPT:REF - eliminate functions which are never referenced
:: SUBSYSTEM - environment for executable
cl /nologo /MTd /Z7 /Gm- /GR- /EHs- /EHa- /Od /Oi /FC /W4 /WX ..\code\client.cpp /link /OPT:REF /SUBSYSTEM:WINDOWS,5.2 /LIBPATH:C:\VulkanSDK\1.0.21.1\Bin User32.lib VKstatic.1.lib

cd ..
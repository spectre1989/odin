mkdir build
cd build

:: Z7 - put debug information in obj instead of pdb
cl /Z7 ..\code\server.cpp
cl /Z7 ..\code\client.cpp

cd ..
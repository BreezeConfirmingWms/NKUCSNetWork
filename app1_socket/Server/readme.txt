1. cmake-version:3.23 visual-studio:2019
2.在当前文件夹的终端运行命令:
cmake -S . -B build -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release
3.cd ./build文件夹看到生成的sln项目文件
4.终端执行命令
msbuild ChatAppServer.sln /p:Configuration=Release /p:Platform=x64
即可在./build/Release文件夹看到可执行文件ChatAppServer.exe
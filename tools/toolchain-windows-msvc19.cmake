# Specify the name of the target operating system
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# toolchain-windows-msvc19.cmake
set(CMAKE_C_COMPILER "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.XX.YY/bin/Hostx64/x64/cl.exe")
set(CMAKE_CXX_COMPILER "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.XX.YY/bin/Hostx64/x64/cl.exe")

set(CMAKE_MAKE_PROGRAM "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/MSBuild/Current/Bin/MSBuild.exe")

# Options spécifiques à MSVC
set(CMAKE_C_FLAGS "/W4 /MD")
set(CMAKE_CXX_FLAGS "/W4 /MD")

# Ajout de la bibliothèque de chemins pour MSVC
set(CMAKE_PREFIX_PATH "C:/Program Files (x86)/Windows Kits/10")


#!/bin/sh

# Cible Linux par defaut (doit correspondre à dist/<BUILD_TARGET> dans CMakeLists.txt)
if [ -z "$BUILD_TARGET" ] ; then
   export BUILD_TARGET=linux-x86_64-trixie
   echo "unspecified BUILD_TARGET using [$BUILD_TARGET]"
fi
export BUILD_TARGET

# Construction des dependances
# if [ $_RECURSE -eq 1 ] && [ -f ${0%/*}/../Description.xml ] ; then
# {
#    python "${0%/*}/Dependencies.py" -d "${0%/*}/../Description.xml" gen
#    if [ $? -eq 1 ] ; then
#       echo "Dependencies.py failed !"
#       exit 1
#    fi
# }
# fi

# Construction de l'application
if [ ! -d ${0%/*}/../build ] ; then
{
   echo "mkdir ${0%/*}/../build"
   mkdir ${0%/*}/../build
}
fi
if [ ! -d ${0%/*}/../dist ] ; then
{
   echo "mkdir ${0%/*}/../dist"
   mkdir ${0%/*}/../dist
}
fi

cd ${0%/*}/../build

if [ ! -f CMakeCache.txt ] ; then
    cmake -G "Unix Makefiles" ..
    if [ $? -ne 0 ]; then
    exit 1;
    fi
fi	

cmake --build . --target libmcp2221a_hid -- -j`nproc`
if [ $? -ne 0 ]; then
   exit 1;
fi

cmake --install .
if [ $? -ne 0 ]; then
   exit 1;
fi

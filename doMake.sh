
if [[ $1 == "debug" ]]; then
  if [[ ! -d "cmake-build-debug" ]]; then
    mkdir cmake-build-debug
  fi
  cd cmake-build-debug

  if [[ $# > 1 ]] && [[ $2 == "all" ]]; then
      rm Makefile
      rm CMakeCache.txt
      rm -rf CMakeFiles
      rm cmake_install.cmake
  fi

  cmake .. -DDEBUG=ON
  make -j3
  cd ..
fi

if [[ $1 == "release" ]]; then
  if [[ ! -d "cmake-build-release" ]]; then
    mkdir cmake-build-release
  fi
  cd cmake-build-release

  if [[ $# > 1 ]] && [[ $2 == "all" ]]; then
      rm Makefile
      rm CMakeCache.txt
      rm -rf CMakeFiles
      rm cmake_install.cmake
  fi

  cmake .. -DDEBUG=OFF
  make -j3
  cd ..
fi


#!/usr/bin/env bash

function build_canary() {
  #Update the submodule and initialize
  git submodule update --init

  #Save current directory
  current_dir="${PWD}"

  build_dir='./build'
  if [ -d $build_dir ]; then
      echo "Deleted folder: ${build_dir}"
      rm -rf $build_dir
  fi

  #Create building folder
  echo "Created building folder: ${build_dir}"
  mkdir $build_dir

  echo "Entering folder: ${build_dir}"
  cd $build_dir

  echo "Start building canary ..."
  if [ $1 -eq 1 ]; then
      cmake .. -DBUILD_TESTING=YES $cmake_gen
  elif [ $1 -eq 2 ]; then
      cmake .. -DBUILD_TESTING=YES -DBUILD_SHARED_LIBS=ON -DCMAKE_CXX_VISIBILITY_PRESET=hidden -DCMAKE_VISIBILITY_INLINES_HIDDEN=1 $cmake_gen
  else
      cmake .. -DCMAKE_BUILD_TYPE=release $cmake_gen
  fi

  #If errors then exit
  if [ "$?" != "0" ]; then
      exit -1
  fi
    
  $make_program $make_flags
  
  #If errors then exit
  if [ "$?" != "0" ]; then
      exit -1
  fi

  #echo "Installing ..."
  #$make_program install

  #Go back to the current directory
  cd $current_dir
  #Ok!
}

make_program=make
make_flags=''
cmake_gen=''
parallel=1

case $(uname) in
 FreeBSD)
  nproc=$(sysctl -n hw.ncpu)
  ;;
 Darwin)
  nproc=$(sysctl -n hw.ncpu)
  ;;
 *)
  nproc=$(nproc)
  ;;
esac

# simulate ninja's parallelism
case nproc in
 1)
  parallel=$(( nproc + 1 ))
  ;;
 2)
  parallel=$(( nproc + 1 ))
  ;;
 *)
  parallel=$(( nproc + 2 ))
  ;;
esac

if [ -f /bin/ninja ]; then
  make_program=ninja
  cmake_gen='-GNinja'
else
  make_flags="$make_flags -j$parallel"
fi

if [ "$1" = "-t" ]; then
  build_canary 1
elif [ "$1" = "-tshared" ]; then
  build_canary 2
else
  build_canary 0
fi

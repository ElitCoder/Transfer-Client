#!/bin/bash

cores=`grep --count ^processor /proc/cpuinfo`

function build {
	if [ $# -ne 0 ]; then
		make clean
	fi
	
	make -j $cores
	
	if [ $? -ne 0 ]; then
		exit 1
	fi
}

if [ $# -eq 0 ]; then
	build
else
	if [ $1 = "clean" ]; then
		build clean
	elif [ $1 = "rel_bin" ]; then
		./clean.sh
		
		# create linux client
		mkdir build; cd build
		cmake ..
		make -j 4
		
		if [ $? -ne 0 ]; then
			exit 1
		fi
		
		cd ../
		mkdir -p rel_bin/linux/
		cp bin/* rel_bin/linux/
		cd rel_bin/linux/
		zip ../transfer_client_linux.zip *
		cd ../../
		rm -r rel_bin/linux/
		
		./clean.sh
		
		# create windows client
		mkdir build; cd build
		cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-windows-mingw32.cmake ..
		make -j 4
		
		if [ $? -ne 0 ]; then
			exit 1
		fi
		
		cd ../
		mkdir -p rel_bin/windows/
		cp bin/* rel_bin/windows/
		cd rel_bin/windows/
		zip ../transfer_client_windows.zip *
		cd ../../
		rm -r rel_bin/windows/
		
		./clean.sh
	elif [ $1 = "run" ]; then
		build
		
		echo $5
		
		cd bin/
		./Transfer-Client "${@:2}"
		cd ../
	fi
fi
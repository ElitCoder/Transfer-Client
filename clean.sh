#!/bin/bash

# clean build
make clean

# clean files
rm -rf bin/files/

# clean unix cmake
rm -rf bin/CMakeFiles/
rm -rf build/

# clean cross platform binaries
rm -f bin/Transfer-Client*
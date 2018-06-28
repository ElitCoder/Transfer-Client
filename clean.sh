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

# clean rel_bin
rm -rf rel_bin/linux/
rm -rf rel_bin/windows/

# clean update files
rm -f bin/client*
rm -f bin/update*
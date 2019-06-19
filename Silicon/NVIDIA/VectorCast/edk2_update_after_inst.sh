#!/bin/bash

#Replace macros to have CHAR16 be an int for vectorcast instrumentation, will reverse before compiling
SCRIPT_DIR=$(dirname "$0")
SOURCE_DIR=$SCRIPT_DIR/../../../..
echo "Undo vectorcast wchar_t size work around"
find $SOURCE_DIR -type f -name *.c -exec sed -i -e 's/typedef int CHAR16/typedef unsigned short CHAR16/' {} \;
find $SOURCE_DIR -type f -name *.c -exec sed -i -e 's/sizeof (CHAR16) == 4/sizeof (CHAR16) == 2/g' {} \;
echo "Apply edk2 patch"
patch -d $SOURCE_DIR/edk2 -l -p1 < $SCRIPT_DIR/edk2.patch
echo "Apply edk2-platforms patch"
patch -d $SOURCE_DIR/edk2-platforms -l -p1 < $SCRIPT_DIR/edk2-platforms.patch
exit 0

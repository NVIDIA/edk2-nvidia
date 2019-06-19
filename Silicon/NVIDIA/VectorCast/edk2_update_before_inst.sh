#!/bin/bash

#Replace macros to have CHAR16 be an int for vectorcast instrumentation, will reverse before compiling
SOURCE_DIR=$(dirname "$0")/../../../..
echo "Apply vectorcast wchar_t size work around"
sed -i -e 's/STATIC_ASSERT (sizeof (CHAR16)  == 2/STATIC_ASSERT (sizeof (CHAR16)  == 4/g' $SOURCE_DIR/edk2/MdePkg/Include/Base.h
sed -i -e 's/typedef\s\+unsigned\s\+short\s\+CHAR16/typedef int CHAR16/g' $SOURCE_DIR/edk2/MdePkg/Include/AArch64/ProcessorBind.h
echo "Done"
exit 0

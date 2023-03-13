#!/bin/bash

cd `dirname $0`
SOFTWARE_VERSION="5.1.6"

#compress directory and compress file name
PKG_DIRNAME="install"
PKG_FINAL_FILENAME="upgrade_pangu_${SOFTWARE_VERSION}_x50.bin"
rm -f upgrade_*.bin
#upgrade file name
PKG_INSTALL_FILE="install.sh"

#makeself pack --notemp
MAKESELF_FILE="./makeself/makeself.sh"
$MAKESELF_FILE $PKG_DIRNAME $PKG_FINAL_FILENAME "package" ./$PKG_INSTALL_FILE

./$PKG_FINAL_FILENAME --list
#makeself check
./$PKG_FINAL_FILENAME --check
    
if [ $? -ne 0 ]; then
    echo "Package failed"
    exit 1;
fi
echo "Package successful........"
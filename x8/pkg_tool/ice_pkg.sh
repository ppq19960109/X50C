#!/bin/bash

cd `dirname $0`
if [ ! -n "$1" ]; then
  echo "IS NULL"
  SOFTWARE_VERSION="8.0.0"
else
  echo "NOT NULL"
  SOFTWARE_VERSION=$1
fi

#compress directory and compress file name
PKG_DIRNAME="ice_install"
PKG_FINAL_FILENAME="ice_upgrade_X8GCZ_${SOFTWARE_VERSION}.bin"
rm -f ice_upgrade_*.bin
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
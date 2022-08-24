#!/bin/bash

cd `dirname $0`
SOFTWARE_VERSION="0.0.1"

#compress directory and compress file name
PKG_DIRNAME="power_install"
PKG_FINAL_FILENAME="power_upgrade_${SOFTWARE_VERSION}_X8.bin"
rm -f power_upgrade_*.bin
#upgrade file name
PKG_INSTALL_FILE="install.sh"

#makeself pack --notemp
MAKESELF_FILE="./makeself/makeself.sh"
$MAKESELF_FILE $PKG_DIRNAME $PKG_FINAL_FILENAME "package" ./$PKG_INSTALL_FILE

./$PKG_FINAL_FILENAME --list
#makeself check
./$PKG_FINAL_FILENAME --check
    
if [ $? -ne 0 ]; then
    echo "power Package failed"
    exit 1;
fi
echo "power Package successful........"
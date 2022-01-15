#!/bin/sh

echo "start install......:"`pwd`

#Add operation permission
chmod -R 777 ./
#kill app
# kill -9 x50app

cp -af root/* /

# for file in ./*
# do
#     if test -d $file
#     then
#         cp -af $file /
#     fi
# done

#delete files
rm -rf *
sync
echo "Successfully installed"

echo "App reboot......"
# reboot

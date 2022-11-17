#!/bin/sh

#echo "Start install pwd:"$(pwd)

#Add operation permission
chmod -R 700 ./
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
rm -f /oem/marssenger/recipe.db
# sed -i 's/reboot=false/reboot=true/' /userdata/.config/Marssenger/X50BCZ.conf
sync
echo "Successfully installed"

#echo "App reboot......"
# reboot

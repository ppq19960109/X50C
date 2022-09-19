cd /oem && ./logoapp
wpa_supplicant -B -i wlan0 -c /data/cfg/wpa_supplicant.conf
#./wakeWordAgent -e gpio &

echo 4 4 1 7 >/proc/sys/kernel/printk
if [ ! -d /userdata/nfs ]; then
    mkdir -p /userdata/nfs
fi
function check_upgrade() {
    for file in $(ls /$1/upgrade*.bin); do
        if [ -f $file ]; then
            echo $file
            sh $file
            rm -rf /$1/upgrade*.bin
            sync
            reboot
            break
        fi
    done
}
check_upgrade userdata
check_upgrade oem

export QT_LINUXFB_DRM_LOGO=/oem/logo.bmp
sh /oem/marssenger/S100Marssenger start

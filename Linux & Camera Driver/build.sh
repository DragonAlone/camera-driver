if [ $# -lt 1 ];
then
        echo "Usage:./build.sh <defconfig|menuconfig|kernel|dtb>"
        exit
fi

case "$1" in
        "defconfig")
                 make  ARCH=arm  imx_v7_defconfig
                ;;
        "menuconfig")
                make ARCH=arm menuconfig
                ;;
        "kernel")
                make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j12
                cp  arch/arm/boot/zImage  ../../tftpboot/
                ;;
        "dtb")
                make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- dtbs
                cp  arch/arm/boot/dts/imx6ull-14x14-smartcar.dtb  ../../tftpboot/
                ;;
        *)
                echo "Usage:./build.sh <defconfig|menuconfig|kernel|dtb>"
                ;;
esac


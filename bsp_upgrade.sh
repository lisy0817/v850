#!/bin/bash

up_kernel=0
up_dtb=0
moreoptions=1

function help(){
    echo "Useage:
    $(basename ${0}) <[kernel(-k)][dtb(d)][all(-a)]>"
}

if [ $# -lt 1 ];then
    help && exit
fi

while [ "${moreoptions}" = 1 -a $# -gt 0 ]; do
	case $1 in
		-h) help; exit ;;
		kernel | -k) up_kernel=1 ;;
		dtb | -d) up_dtb=1 ;;
		all | -a) up_kernel=1; up_dtb=1;;
		*) moreoptions=0 ; help &&exit ;;
	esac
	[ "${moreoptions}" = 0 ] && [ $# -gt 1 ] && help && exit
	[ "${moreoptions}" = 1 ] && shift
done

if [ ${up_kernel} -eq 0 -a ${up_dtb} -eq 0 ];then
	echo "upgrade don't needed"
	exit 1
fi

partition=`system_upgrade -getkernel /dev/mmcblk0p1 | awk -F ':' '{print $4}'`

if [ ${up_kernel} -ne 0  ] ;then
	sync
	if [ -e uImage ];then
		if [ ${partition} -eq 0 ];then
			dd if=uImage of=/dev/mmcblk0 bs=512 seek=32768 conv=fsync
		else
			dd if=uImage of=/dev/mmcblk0 bs=512 seek=65536 conv=fsync
		fi
		if [ $? -eq 0 ];then
			echo "---upgrade kernel success---"
		else
			echo "---upgrade kernel fail---"
		fi
	else
		echo "---uImage not exist---"
	fi
fi

if [ ${up_dtb} -ne 0  ] ;then
	sync
	if [ -e imx6dl-sabresd.dtb ];then
		if [ ${partition} -eq 0 ];then
			dd if=imx6dl-sabresd.dtb of=/dev/mmcblk0 bs=512 seek=44800 conv=fsync
		else
			dd if=imx6dl-sabresd.dtb of=/dev/mmcblk0 bs=512 seek=77568 conv=fsync
		fi
		if [ $? -eq 0 ];then
			echo "---upgrade dtb success---"
		else
			echo "---upgrade dtb fail---"
		fi
	else
		echo "---imx6dl-sabresd.dtb not exist---"
	fi
fi




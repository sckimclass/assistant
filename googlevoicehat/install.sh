#!/bin/bash

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root (use sudo)" 1>&2
   exit 1
fi

OVERLAYS=/boot/overlays

# Check if enough space on /boot volume
boot_line=$(df -BM | grep /boot | head -n 1)
MIN_BOOT_SPC=25 # MegaBytes
if [ "x${boot_line}" = "x" ]; then
  echo "Warning: /boot volume not found .."
else
  boot_space=$(echo $boot_line | awk '{print $4;}')
  free_space=$(echo "${boot_space%?}")
  unit="${boot_space: -1}"
  if [[ "$unit" != "M" ]]; then
    echo "Warning: /boot volume not found .."
  elif [ "$free_space" -lt "$MIN_BOOT_SPC" ]; then
    echo "Error: Not enough space left ($boot_space) on /boot"
    echo "       at least $MIN_BOOT_SPC MB required"
    exit 1
  fi
fi

ver="0.3"
uname_r=$(uname -r)
arch_r=$(dpkg --print-architecture)

_VER_RUN=
function get_kernel_version() {
  local ZIMAGE IMG_OFFSET

  _VER_RUN=""
  [ -z "$_VER_RUN" ] && {
    ZIMAGE=/boot/kernel.img
    [ -f /boot/firmware/vmlinuz ] && ZIMAGE=/boot/firmware/vmlinuz
    IMG_OFFSET=$(LC_ALL=C grep -abo $'\x1f\x8b\x08\x00' $ZIMAGE | head -n 1 | cut -d ':' -f 1)
    _VER_RUN=$(dd if=$ZIMAGE obs=64K ibs=4 skip=$(( IMG_OFFSET / 4)) 2>/dev/null | zcat | grep -a -m1 "Linux version" | strings | awk '{ print $3; }')
  }
  echo "$_VER_RUN"
  return 0
}

function check_kernel_headers() {
  VER_RUN=$(get_kernel_version)
  VER_HDR=$(dpkg -L raspberrypi-kernel-headers | egrep -m1 "/lib/modules/[^-]+/build" | awk -F'/' '{ print $4; }')
  [ "X$VER_RUN" == "X$VER_HDR" ] && {
    echo "alan Ab"
    return 0
  }
  VER_HDR=$(dpkg -L linux-headers-$VER_RUN | egrep -m1 "/lib/modules/[[:print:]]+/build" | awk -F'/' '{ print $4; }')
  [ "X$VER_RUN" == "X$VER_HDR" ] && {
    echo "alan AB"
    return 0
  }

  # echo RUN=$VER_RUN HDR=$VER_HDR
  echo " !!! Your kernel version is $VER_RUN"
  echo "     Couldn't find *** corresponding *** kernel headers with apt-get."
  echo "     This may happen if you ran 'rpi-update'."
  echo " Choose  *** y *** to revert the kernel to version $VER_HDR and continue."
  echo " Choose  *** N *** to exit without this driver support, by default."
  read -p "Would you like to proceed? (y/N)" -n 1 -r -s
  echo
  if ! [[ $REPLY =~ ^[Yy]$ ]]; then
    exit 1;
  fi

  apt-get -y --reinstall install raspberrypi-kernel
}

function download_install_debpkg() {
  local prefix name r pkg status _name
  prefix=$1
  name=$2
  pkg=${name%%_*}

  status=$(dpkg -l $pkg | tail -1)
  _name=$(  echo "$status" | awk '{ printf "%s_%s_%s", $2, $3, $4; }')
  status=$(echo "$status" | awk '{ printf "%s", $1; }')

  if [ "X$status" == "Xii" -a "X${name%.deb}" == "X$_name" ]; then
    echo "debian package $name already installed."
    return 0
  fi

  for (( i = 0; i < 3; i++ )); do
    wget $prefix$name -O /tmp/$name && break
  done
  dpkg -i /tmp/$name; r=$?
  rm -f /tmp/$name
  return $r
}

compat_kernel=
keep_kernel=
# parse commandline options
while [ ! -z "$1" ] ; do
  case $1 in
  -h|--help)
    usage
    ;;
  --compat-kernel)
    compat_kernel=Y
    ;;
  --keep-kernel)
    keep_kernel=Y
    ;;
  esac
  shift
done

if [ "X$keep_kernel" != "X" ]; then
  FORCE_KERNEL=$(dpkg -s raspberrypi-kernel | awk '/^Version:/{printf "%s\n",$2;}')
  echo -e "\n### Keep current system kernel not to change"
elif [ "X$compat_kernel" != "X" ]; then
  echo -e "\n### will compile with a compatible kernel..."
else
  FORCE_KERNEL=""
  echo -e "\n### will compile with the latest kernel..."
fi
[ "X$FORCE_KERNEL" != "X" ] && {
  echo -e "The kernel & headers use package version: $FORCE_KERNEL\r\n\r\n"
}

function install_kernel() {
  local _url _prefix

  # Instead of retrieving the lastest kernel & headers
  [ "X$FORCE_KERNEL" == "X" ] && {
    # Raspbian kernel packages
    apt-get -y --force-yes install raspberrypi-kernel-headers raspberrypi-kernel || {
      # Ubuntu kernel packages
      apt-get -y install linux-raspi linux-headers-raspi linux-image-raspi
    }
  } || {
    # We would like to a fixed version
    KERN_NAME=raspberrypi-kernel_${FORCE_KERNEL}_${arch_r}.deb
    HDR_NAME=raspberrypi-kernel-headers_${FORCE_KERNEL}_${arch_r}.deb
    _url=$(apt-get download --print-uris raspberrypi-kernel | sed -nre "s/'([^']+)'.*$/\1/g;p")
    _prefix=$(echo $_url | sed -nre 's/^(.*)raspberrypi-kernel_.*$/\1/g;p')

    download_install_debpkg "$_prefix" "$KERN_NAME" && {
      download_install_debpkg "$_prefix" "$HDR_NAME"
    } || {
      echo "Error: Install kernel or header failed"
      exit 2
    }
  }
}

# update and install required packages
which apt &>/dev/null; r=$?
if [[ $r -eq 0 ]]; then
  echo -e "\n### Install required tool packages"
  apt update -y
  apt-get -y install dkms git i2c-tools libasound2-plugins
fi

if [[ $r -eq 0 ]]; then
  echo -e "\n### Install required kernel package"
  install_kernel
  # rpi-update checker
  check_kernel_headers
fi

# locate currently installed kernels (may be different to running kernel if
# it's just been updated)
base_ver=$(get_kernel_version)
base_ver=${base_ver%%[-+]*}
# kernels="${base_ver}+ ${base_ver}-v7+ ${base_ver}-v7l+"
# select exact kernel postfix
kernels=${base_ver}$(echo $uname_r | sed -re 's/^[0-9.]+(.*)/\1/g')

function install_module {
  local _i

  src=$1
  mod=$2

  echo $src
  echo $mod

  mkdir -p /usr/src/$mod-$ver
  cp -a $src/* /usr/src/$mod-$ver/

  dkms add -m $mod -v $ver
  for _i in $kernels; do
    dkms build -k $_i -m $mod -v $ver && {
      dkms install --force -k $_i -m $mod -v $ver
    } || {
      echo "Can't compile with this kernel, aborting"
      echo "Please try to compile with the option --compat-kernel"
      exit 1
    }
  done

  mkdir -p /var/lib/dkms/$mod/$ver/$marker
}

echo -e "\n### Install sound card driver"
install_module "./" "googlevoicehat"

echo -e "\n### Install device tree overlays"
cp -v googlevoicehat-soundcard.dtbo $OVERLAYS

#!/bin/bash

ARCH=`uname -m`

if [[ $ARCH == "arm6"* ]]; then export "VIS_COMPILER_ARCH"="armv6"; fi
if [[ $ARCH == "armv6"* ]]; then export "VIS_COMPILER_ARCH"="armv6"; fi
if [[ $ARCH == "armv7"* ]]; then export "VIS_COMPILER_ARCH"="armv6"; fi

make clean
make
sudo make install

if [ -z "$XDG_CONFIG_HOME" ]
then
    CONFIG_DIR=$HOME/.config/vis
else
    CONFIG_DIR=$XDG_CONFIG_HOME/vis
fi

#create config directory
mkdir -p $CONFIG_DIR/colors

#copy over example files
cp -u examples/config $CONFIG_DIR/
cp -u examples/rainbow $CONFIG_DIR/colors/rainbow
cp -u examples/basic_colors $CONFIG_DIR/colors/basic_colors

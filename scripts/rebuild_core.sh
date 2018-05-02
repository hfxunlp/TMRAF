#!/bin/bash

echo "build for psg1218"
cp configs/templates/psg1218_mini.config .config
cp configs/boards/uclibc-mipsel.config configs/boards/PSG1218/libc.config
./clear_tree
./build_firmware
mv images/PSG1218_*.trx /media/Storage/workspace/router/firmware/

echo "build for newifi mini"
cp configs/templates/newifimini_mini.config .config
cp configs/boards/uclibc-mipsel.config configs/boards/NEWIFI-MINI/libc.config
./clear_tree
./build_firmware
mv images/NEWIFI-MINI_*.trx /media/Storage/workspace/router/firmware/

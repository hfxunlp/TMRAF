#!/bin/bash

echo "build for psg1218"
cp configs/templates/psg1218_mini.config .config
cp configs/boards/uclibc-mipsel.config configs/boards/PSG1218/libc.config
./clear_tree
./build_firmware
mv images/PSG1218_*.trx ~/router/firmware/

echo "build for newifi mini"
cp configs/templates/newifimini_mini.config .config
cp configs/boards/uclibc-mipsel.config configs/boards/NEWIFI-MINI/libc.config
./clear_tree
./build_firmware
mv images/NEWIFI-MINI_*.trx ~/router/firmware/

echo "build for newifi y1s"
cp configs/templates/newifiy1s_mini.config .config
cp configs/boards/uclibc-mipsel.config configs/boards/NEWIFI-Y1S/libc.config
cp configs/boards/NEWIFI-Y1S/Gkernel-3.4.x.config configs/boards/NEWIFI-Y1S/kernel-3.4.x.config
cp configs/boards/NEWIFI-Y1S/Gboard.h configs/boards/NEWIFI-Y1S/board.h
./clear_tree
./build_firmware
mv images/NEWIFI-Y1S_*.trx ~/router/firmware/


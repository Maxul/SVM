#!/bin/sh

umount /tmp/share
rm -rf /tmp/share

rm /opt/share.img

dd if=/dev/zero of=/opt/share.img bs=1M count=4

mkfs.ext4 /opt/share.img

mkdir /tmp/share
mount -o loop /opt/share.img /tmp/share

chown -R maxul:maxul /tmp/share
chown -R maxul:maxul /opt/share.img

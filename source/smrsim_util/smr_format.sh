# Copyright (C) 2014-2015, Western Digital Technologies, Inc. <copyrightagent@wdc.com>
# Platform Development Group
#!/bin/bash

usage ()
{
   echo "Usage: $0 [-z] -d device|partition|loop"
   echo "   -z    Print available smrsim capacity in zones instead of 512-byte sectors"
}

show_zones=0
smr_device=""

while getopts ":zd:" opt; do
   case $opt in
      d)
         smr_device=${OPTARG}
         ;;
      z)
         show_zones=1
         ;;
      \?)
         echo "Invalid option: $OPTARG" 1>&2
         ;;
   esac
done

if [[ $EUID -ne 0 ]]; then
   echo "You must be a root user (e.g. sudo $0)." 1>&2
   exit 1
fi

if [[ x"${smr_device}" == x"" ]]; then
   usage
   exit 1
fi

# Computer number of 256 MB zones leaving room for persistence data after last zone.
device_size_bytes=`blockdev --getsize64 ${smr_device}`
zones=$(bc <<< "($device_size_bytes-1)/(256*1024*1024)")
ublk=$(bc <<< "(256*1024*1024*(($device_size_bytes-1)/(256*1024*1024)))/512")

# Initialize 2 MB at the end of the device for SMRSim persistence data.
dd if=/dev/zero of=${smr_device} bs=4096 seek=$((ublk+1)) count=512 2> /dev/null 1> /dev/null

if [[ $show_zones -eq 1 ]]; then
   echo "$zones"
else
   echo "$ublk"
fi


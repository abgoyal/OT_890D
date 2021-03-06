#!/bin/sh
verify () {
	if [ ! -f "$1" ]; then
		echo ""                                                   1>&2
		echo " *** Missing file: $1"                              1>&2
		echo ' *** You need to run "make" before "make install".' 1>&2
		echo ""                                                   1>&2
		exit 1
 	fi
}

# Make sure the files actually exist
verify "$2"
verify "$3"

# User may have a custom install script

if [ -x ~/bin/${CROSS_COMPILE}installkernel ]; then exec ~/bin/${CROSS_COMPILE}installkernel "$@"; fi
if [ -x /sbin/${CROSS_COMPILE}installkernel ]; then exec /sbin/${CROSS_COMPILE}installkernel "$@"; fi

# Default install - same as make zlilo

if [ -f $4/vmlinuz ]; then
	mv $4/vmlinuz $4/vmlinuz.old
fi

if [ -f $4/System.map ]; then
	mv $4/System.map $4/System.old
fi

cat $2 > $4/vmlinuz
cp $3 $4/System.map

if [ -x /sbin/lilo ]; then
       /sbin/lilo
elif [ -x /etc/lilo/install ]; then
       /etc/lilo/install
else
       sync
       echo "Cannot find LILO."
fi

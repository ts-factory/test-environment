#!/bin/sh

# gct-init [-test-map mapfile] [libdir]

# WARNING:  This file is generated from gct-init.sh.

MAPFILE=gct-map
if [ "x$1" = "x-test-map" ]
then
	shift
	if [ "x$1" = "x" ]
	then
		echo "gct-init:  -test-map takes an argument."
		exit 0
	fi
	MAPFILE=$1
	shift
fi

if [ "x$1" = "x" ]
then
	# LIBDIR is replaced with the true library directory by the makefile.
	true_libdir=LIBDIR
else
	true_libdir=$1
fi

if [ ! -d $true_libdir ]
then
	echo "Configuration error:  library directory $true_libdir does not exist?"
	exit 1
fi
if [ ! -f $true_libdir/gct-ps-defs.G ]
then
	echo "Configuration error:  library $true_libdir is empty?"
	exit 1
fi

INSTRUMENTATION_START_TIME=`date`

echo "#define GCT_NUM_CONDITIONS 0" > gct-ps-defs.h
echo "#define GCT_NUM_RACE_GROUPS 0" >> gct-ps-defs.h
echo "#define GCT_NUM_FILES 0" >> gct-ps-defs.h
/bin/rm -f gct-ps-defs.c
/bin/cp $true_libdir/gct-ps-defs.G gct-ps-defs.c
chmod a+w gct-ps-defs.c
echo "char *Gct_timestamp = \"$INSTRUMENTATION_START_TIME\";" >> gct-ps-defs.c

if [ ! -f gct-write.c ]
then
  /bin/cp $true_libdir/gct-write.G gct-write.c
fi

if [ ! -f gct-defs.h ] ; then /bin/cp $true_libdir/gct-defs.h gct-defs.h ; fi

if [ -f gct-rscript ]
then
  /bin/rm -f gct-rscript.bk
  /bin/mv gct-rscript gct-rscript.bk
fi

if [ -f $MAPFILE ] ; then /bin/rm $MAPFILE ; fi
# Note:  If version changes, must also change string in gct-const.h. 
echo '!GCT-Mapfile-Version: 2.0' >> $MAPFILE
echo "!Timestamp: $INSTRUMENTATION_START_TIME" >> $MAPFILE
exit 0

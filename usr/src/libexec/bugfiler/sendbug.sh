#!/bin/sh -
#
# Copyright (c) 1983 Regents of the University of California.
# All rights reserved.  The Berkeley software License Agreement
# specifies the terms and conditions for redistribution.
#
#	@(#)sendbug.sh	5.3 (Berkeley) 85/08/09
#
# Create a bug report and mail to '4bsd-bugs'.
#

TEMP=/tmp/bug$$
FORMAT=/usr/ucb/bugformat

: ${BUGADDR=4bsd-bugs@ucbarpa.BERKELEY.EDU}
: ${EDITOR=/usr/ucb/vi}

trap '/bin/rm -f $TEMP' 0 1 2 3 13 15

/bin/cp $FORMAT $TEMP
if $EDITOR $TEMP
then
	if cmp -s $FORMAT $TEMP
	then
		echo "File not changed, no bug report submitted."
		exit
	fi
	case "$#" in
	0) /usr/lib/sendmail -t -oi $BUGADDR  < $TEMP ;;
	*) /usr/lib/sendmail -t -oi "$@" < $TEMP ;;
	esac
fi

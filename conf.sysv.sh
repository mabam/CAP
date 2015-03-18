#!/bin/sh
# $Author: djh $ $Date: 1994/10/10 08:54:05 $
# $Header: /mac/src/cap60/RCS/conf.sysv.sh,v 2.2 1994/10/10 08:54:05 djh Rel djh $
# $Revision: 2.2 $
# CAP System V configuration aid shell script.  This ain't perfect,
# but it's a start
#
# execute with /bin/sh conf.sysv.sh if your system won't run it (ksh is okay
# too)
# 
# Usage: conf.sysv.sh [output file name]
#
mydir=`pwd`
PCAT=/bin/cat
PGREP=grep
if [ -f /usr/ccs/bin/nm ]; then
  PNM="/usr/ccs/bin/nm -p"
else
  PNM=/bin/nm
fi
ccompiler=cc
export PGREP
# define to sh or /bin/sh if shell scripts can't be "executed" for some reason
USESH=""

needfcntldoth=0
usechown=0

echo "This is the CAP System V configuration aid script.  This will"
echo "attempt to help you generate "define"s suitable for inclusion in"
echo "netat/sysvcompat.h for a particular machine"
echo
echo "Please refer to NOTES and PORTING before you run if you haven't"
echo "already" 
echo
echo

if [ -f /bin/uname ]; then
 uname > /dev/null 2>/dev/null
 sysv=$?
else
 sysv=1
fi
if [ $sysv -ne 0 ]; then
  echo "Your system is probably not a System V based machine, but"
  echo "we shall proceed in any event"
fi
echo
echo "Seeing if we need to include fcntl.h for definitions normally"
echo "found in <sys/file.h> under BSD"
echo "Temporary files: /tmp/csv$$.c, csv$$.o"
echo "[Hit carriage return to continue]"
read ans
trap "
echo Exiting... Wait..
if [ -f /tmp/csv$$.c ]; then rm -f /tmp/csv$$.c; fi; 
if [ -f csv$$.o ]; then rm -f csv$$.o fi
exit 255" 2
if [ -f /usr/include/sys/file.h ]; then
 exec > /tmp/csv$$.c
 echo "#include <sys/file.h>"
 echo "main(){int i = O_RDONLY|O_WRONLY|O_RDWR|O_TRUNC;}"
 exec > /dev/tty
 ${ccompiler} -c /tmp/csv$$.c > /dev/null 2>/dev/null
 rc=$?
else
 rc=1
fi
if [ $rc -ne 0 ]; then
 echo
 echo "We will include <fcntl.h> to get mappings for O_RDONLY, etc"
 echo "since they don't seem to be in <sys/file.h>"
 needfcntldoth=1
else
 echo
 echo "Shouldn't need fcntl.h"
fi
if [ -f /tmp/csv$$.c ]; then rm -f /tmp/csv$$.c; fi
if [ -f csv$$.o ]; then rm -f csv$$.o; fi
trap 2
echo
echo "Seeing if we can use chown to give away files"
echo "Temporary files: /tmp/csv$$.c, xsv$$"
echo "[Hit carriage return to continue]"
read ans
trap "
echo Exiting... Wait...
if [ -f /tmp/csv$$.c ]; then rm -f /tmp/csv$$.c; fi; 
if [ -f xsv$$ ]; then rm -f xsv$$ fi
exit 255" 2
exec > /tmp/csv$$.c
echo "main(){int u=getuid(),g=getgid();u++;exit(chown(\"/tmp/csv$$.c\",u,g));}"
exec > /dev/tty
${ccompiler} -o xsv$$ /tmp/csv$$.c > /dev/null 2>/dev/null
rc=$?
if [ $rc -eq 0 -a -f xsv$$ ]; then
  ./xsv$$
  rc=$?
else
 rc=1
fi
if [ $rc -eq 0 ]; then
 echo
 echo "Yes, we can use chown to give away files."
 usechown=1
else
 echo
 echo "Can't use chown"
fi
if [ -f /tmp/csv$$.c ]; then rm -f /tmp/csv$$.c; fi
if [ -f xsv$$ ]; then rm -f xsv$$; fi
trap 2
echo
echo "Checking for various system calls and required header files for"
echo "System V compatibility"
echo "Temporary files: defines.tmp (not erased), /tmp/csv$$"
echo
echo "[Hit carriage return to continue]"
read ans
echo
trap "
echo Exiting... Wait...
if [ -f /tmp/csv$$ ]; then rm -f /tmp/csv$$; fi; exit 255" 2
if [ -f /lib/386/Slibc.a ]; then
  echo "Getting name list from /lib/386/Slib[cx].a..."
  ${PNM} /lib/386/Slibc.a > /tmp/csv$$
  ${PNM} /lib/386/Slibx.a >> /tmp/csv$$
else
  echo "Getting name list from /lib/libc.a..."
  ${PNM} /lib/libc.a > /tmp/csv$$
fi
names=$?
if [ $names -ne 0 ]; then
 echo "Couldn't get the name list!"
else
 echo "Done, running function configuration"
 ${USESH} ./conf.func.sh /tmp/csv$$ conf.sysv.lst cpp defines.tmp
 rc=$?
 if [ $rc -eq 1 ]; then
   if [ -z "${USESH}" ]; then
     sh conf.func.sh /tmp/csv$$ conf.sysv.lst cpp defines.tmp
   fi
 fi
 echo "Done."
fi
rm -f /tmp/csv$$
trap 2
# now setup
if [ -z "$1" ]; then
 of=sysv.cpp
else
 of=$1
fi
echo
echo "[Hit carriage return to continue]"
read ans
echo
if [ -f ${of} ]; then
  echo "Getting ready to overwrite existing ${of}"
else
  echo "Getting ready to create ${of}"
fi
echo "[Hit carriage return to continue]"
read ans
echo "Creating ${of}"
exec > ${of}
echo "If all the defines are commented out, then you really don't need"
echo "to put the output into sysvcompat.h"
echo
cat defines.tmp
if [ $needfcntldoth -eq 1 ]; then
 echo "# define NEEDFCNTLDOTH /* if need fcntl.h for O_... */"
else
 echo "/* # define NEEDFCNTLDOTH */ /* if need fcntl.h for O_... */"
fi
if [ $usechown -eq 1 ]; then
 echo "# define USECHOWN /* sysv allows us */"
else
 echo "/* # define USECHOWN */ /* sysv allows us */"
fi
exec > /dev/tty
echo "${of} configured"
echo
echo "Done.  ${of} now contains a set of defines suitable for inclusion"
echo "in sysvcompat.h.  You should include them with an ifdef approriate"
echo "for your machine"
echo
echo "If all the defines are commented out, then you really don't need"
echo "to put the output into sysvcompat.h"



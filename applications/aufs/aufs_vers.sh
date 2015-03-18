#!/bin/sh
echo "int aufs_version[2] = {" $2 "," $4 "};" > $6
if [ -f /usr/local/fdate ]; then
 echo "char *aufs_versiondate = \""`fdate "%L %d, 19%y"`"\";" >> $6
else
 echo "char *aufs_versiondate = \"" `date` "\";" >> $6
fi
if [ $5 != "useold" ]; then
 newver=`expr $4 + 1`
 echo $1 $2 $3 $newver > nv$$
 mv nv$$ $5
else
 echo "No version updating"
fi




#!/bin/sh -e

# Modified from Debian's add-shell to work on OS X

if test $# -eq 0
then
	echo usage: $0 shellname [shellname ...]
	exit 1
fi

scriptname=`basename "$0"`
if [[ $UID -ne 0 ]]; then
     echo "${scriptname} must be run as root"
     exit 1
fi

file=/etc/shells
# I want this to be GUARANTEED to be on the same filesystem as $file
tmpfile=${file}.tmp

set -o noclobber

trap "rm -f $tmpfile" EXIT

if ! cat $file > $tmpfile
then
        cat 1>&2 <<EOF
Either another instance of $0 is running, or it was previously interrupted.
Please examine ${tmpfile} to see if it should be moved onto ${file}.
EOF
        exit 1
fi

# Append a newline if it doesn't exist
if [ "$(tail -c1 "$tmpfile"; echo x)" != $'\nx' ]; then
    echo "" >> "$tmpfile"
fi

for i
do
        if ! grep -q "^${i}$" "$tmpfile"
        then
                echo $i >> "$tmpfile"
        fi
done

chmod 0644 $tmpfile
chown root:wheel $tmpfile

mv $tmpfile $file

trap "" EXIT
exit 0

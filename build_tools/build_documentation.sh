#!/bin/sh

# This script is run as part of the build process
# It looks for doc_src, and then builds documentation out of it
# into either help_doc or $BUILT_PRODUCTS_DIR (if set)

# If running from Xcode, the fish directory is in SRCROOT;
# otherwise assume it's the current directory
FISHDIR=`pwd`
if test -n "$SRCROOT"; then
    FISHDIR="$SRCROOT"
fi

# Make sure doc_src is found
if test ! -d "${FISHDIR}/doc_src"; then
	echo >&2 "doc_src not found in '${FISHDIR}'"
	exit 1
fi

# Make sure doxygen is found
DOXYGENPATH=`command -v doxygen`
if test -z "$DOXYGENPATH" ; then
    for i in /usr/local/bin/doxygen /opt/bin/doxygen; do
    	if test -f "$i"; then
    	    DOXYGENPATH="$i"
    	    break
    	fi
    done
fi

if test -z "$DOXYGENPATH"; then
    echo >&2 "doxygen is not installed, so documentation will not be built."
    exit 0
fi

# Determine where our output should go
OUTPUTDIR="${FISHDIR}/help_doc"
if test -n "$BUILT_PRODUCTS_DIR"; then
	OUTPUTDIR="$BUILT_PRODUCTS_DIR" 
fi
mkdir -p "${OUTPUTDIR}"


# Make a temporary directory
TMPLOC=`mktemp -d -t fish_doc_build_XXXXXX` || { echo >&2 "Could not build documentation because mktemp failed"; exit 1; }

# Copy stuff to the temp directory
for i in "$FISHDIR"/doc_src/*.txt; do
	DOXYFILE=$TMPLOC/`basename $i .txt`.doxygen
	echo  "/** \page" `basename $i .txt` >$DOXYFILE;
	cat $i >>$DOXYFILE;
	echo "*/" >>$DOXYFILE;
done

# Make some extra stuff to pass to doxygen
# Input is kept as . because we cd to the input directory beforehand
# This prevents doxygen from generating "documentation" for intermediate directories
DOXYPARAMS=$(cat <<EOF
PROJECT_NUMBER=2.0.0
INPUT=.
OUTPUT_DIRECTORY=$OUTPUTDIR
QUIET=YES
EOF
);

# echo "$DOXYPARAMS"

# Clear out the output directory first
find "${OUTPUTDIR}" -name "*.1" -delete

# Run doxygen
cd "$TMPLOC"
(cat "${FISHDIR}/Doxyfile.help" ; echo "$DOXYPARAMS";) | "$DOXYGENPATH" - 

# Remember errors
RESULT=$?

cd "${OUTPUTDIR}/man/man1/"
if test "$RESULT" = 0 ; then
	# Postprocess the files
	for i in "$FISHDIR"/doc_src/*.txt; do
		# It would be nice to use -i here for edit in place, but that is not portable 
		CMD_NAME=`basename "$i" .txt`;
		sed -e "s/\(.\)\\.SH/\1/" -e "s/$CMD_NAME *\\\\- *\"\(.*\)\"/\1/" "${CMD_NAME}.1" > "${CMD_NAME}.1.tmp"
		mv "${CMD_NAME}.1.tmp" "${CMD_NAME}.1"
	done
fi

# Destroy TMPLOC
echo "Cleaning up '$TMPLOC'"
rm -Rf "$TMPLOC"

if test $RESULT == 0; then
    # Tell the user what we did
    echo "Output man pages into '${OUTPUTDIR}'"
else
    echo "Doxygen failed. See the output log for details."
fi
exit $RESULT

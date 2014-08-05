#!/bin/sh

# This script is run as part of the build process

if test $# -eq 0
then
	# Use fish's defaults
	DOXYFILE=Doxyfile.help
	INPUTDIR=doc_src
	OUTPUTDIR=share
	echo "Using defaults: $0 ${DOXYFILE} ${INPUTDIR} ${OUTPUTDIR}"
elif test $# -eq 3
then
	DOXYFILE="$1"
	INPUTDIR="$2"
	OUTPUTDIR="$3"
else
	echo "Usage: $0 doxygen_file input_directory output_directory"
	exit 1
fi

# Determine which man pages we don't want to generate.
# on OS X, don't make a man page for open, since we defeat fish's open function on OS X.
CONDEMNED_PAGES=
if test `uname` = 'Darwin'; then
	CONDEMNED_PAGES="$CONDEMNED_PAGES open.1"
fi

# Helper function to turn a relative path into an absolute path
resolve_path()
{
	D=`command dirname "$1"`
	B=`command basename "$1"`
	echo "`cd \"$D\" 2>/dev/null && pwd || echo \"$D\"`/$B"
}

# Expand relative paths
DOXYFILE=`resolve_path "$DOXYFILE"`
INPUTDIR=`resolve_path "$INPUTDIR"`
INPUTFILTER=`resolve_path "$INPUT_FILTER"`
OUTPUTDIR=`resolve_path "$OUTPUTDIR"`

echo "      doxygen file: $DOXYFILE"
echo "   input directory: $INPUTDIR"
echo "      input filter: $INPUTFILTER"
echo "  output directory: $OUTPUTDIR"
echo "          skipping: $CONDEMNED_PAGES"

# Make sure INPUTDIR is found
if test ! -d "$INPUTDIR"; then
	echo >&2 "Could not find input directory '${INPUTDIR}'"
	exit 1
fi

# Make sure doxygen is found
DOXYGENPATH=`command -v doxygen`
if test -z "$DOXYGENPATH" ; then
    for i in /usr/local/bin/doxygen /opt/bin/doxygen /Applications/Doxygen.app/Contents/Resources/doxygen ~/Applications/Doxygen.app/Contents/Resources/doxygen ; do
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

# Check we have the lexicon filter
if test -z "$INPUT_FILTER"; then
    echo >&2 "Lexicon filter is not available. Continuing without."
    INPUTFILTER=''
fi

# Determine where our output should go
if ! mkdir -p "${OUTPUTDIR}" ; then
    echo "Could not create output directory '${OUTPUTDIR}'"
fi

# Make a temporary directory
TMPLOC=`mktemp -d -t fish_doc_build_XXXXXX` || { echo >&2 "Could not build documentation because mktemp failed"; exit 1; }

# Copy stuff to the temp directory
for i in "$INPUTDIR"/*.txt; do
	INPUTFILE=$TMPLOC/`basename $i .txt`.doxygen
	echo  "/** \page" `basename $i .txt` > $INPUTFILE
	cat $i >>$INPUTFILE
	echo "*/" >>$INPUTFILE
done

# Make some extra stuff to pass to doxygen
# Input is kept as . because we cd to the input directory beforehand
# This prevents doxygen from generating "documentation" for intermediate directories
DOXYPARAMS=$(cat <<EOF
PROJECT_NUMBER=$PROJECT_NUMBER
INPUT_FILTER=$INPUTFILTER
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
(cat "${DOXYFILE}" ; echo "$DOXYPARAMS";) | "$DOXYGENPATH" -

# Remember errors
RESULT=$?

cd "${OUTPUTDIR}/man/man1/"
if test "$RESULT" = 0 ; then

	# Postprocess the files
	for i in "$INPUTDIR"/*.txt; do
		# It would be nice to use -i here for edit in place, but that is not portable
		CMD_NAME=`basename "$i" .txt`;
		sed < ${CMD_NAME}.1 > ${CMD_NAME}.1.tmp \
            -e "/.SH \"$CMD_NAME/d" \
            -e "s/^$CMD_NAME * \\\- \([^ ]*\) /\\\fB\1\\\fP -/"
		mv "${CMD_NAME}.1.tmp" "${CMD_NAME}.1"
	done

	# Erase condemned pages
	rm -f $CONDEMNED_PAGES
fi

# Destroy TMPLOC
echo "Cleaning up '$TMPLOC'"
rm -Rf "$TMPLOC"

if test "$RESULT" = 0; then
    # Tell the user what we did
    echo "Output man pages into '${OUTPUTDIR}'"
else
    echo "Doxygen failed. See the output log for details."
fi
exit $RESULT

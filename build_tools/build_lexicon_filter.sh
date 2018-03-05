#!/bin/sh

# Builds the lexicon filter
# Usage: build_lexicon_filter.sh FUNCTIONS_DIR COMPLETIONS_DIR lexicon_filter.in [SED_BINARY] > lexicon_filter

set -e

# To enable the lexicon filter, we first need to be aware of what fish
# considers to be a command, function, or external binary. We use
# command_list_toc.txt for the base commands. Scan the share/functions
# directory for other functions, some of which are mentioned in the docs, and
# use /share/completions to find a good selection of binaries. Additionally,
# colour defaults from __fish_config_interactive to set the docs colours when
# used in a 'cli' style context.
rm -f lexicon.tmp lexicon_catalog.tmp lexicon_catalog.txt lexicon.txt

FUNCTIONS_DIR=${1}
FUNCTIONS_DIR_FILES=${1}/*.fish
COMPLETIONS_DIR_FILES=${2}/*.fish
LEXICON_FILTER_IN=${3}

SED=${4:-$(command -v sed)}

# Scan sources for commands/functions/binaries/colours. If GNU sed was portable, this could be much smarter.
$SED <command_list_toc.txt >>lexicon.tmp -n \
	-e "s|^.*>\([a-z][a-z_]*\)</a>|'\1'|w lexicon_catalog.tmp" \
	-e "s|'\(.*\)'|bltn \1|p"; mv lexicon_catalog.tmp lexicon_catalog.txt
printf "%s\n" ${COMPLETIONS_DIR_FILES} | $SED -n \
	-e "s|[^ ]*/\([a-z][a-z_-]*\).fish|'\1'|p" | grep -F -vx -f lexicon_catalog.txt | $SED >>lexicon.tmp -n \
	-e 'w lexicon_catalog.tmp' \
	-e "s|'\(.*\)'|cmnd \1|p"; cat lexicon_catalog.tmp >> lexicon_catalog.txt;
printf "%s\n" ${FUNCTIONS_DIR_FILES} | $SED -n \
	-e "s|[^ ]*/\([a-z][a-z_-]*\).fish|'\1'|p" | grep -F -vx -f lexicon_catalog.txt | $SED >>lexicon.tmp -n \
	-e 'w lexicon_catalog.tmp' \
	-e "s|'\(.*\)'|func \1|p";
$SED < ${FUNCTIONS_DIR}/__fish_config_interactive.fish >>lexicon.tmp -n  \
	-e '/set_default/s/.*\(fish_[a-z][a-z_]*\).*$$/clrv \1/p'; \
$SED < ${LEXICON_FILTER_IN} >>lexicon.tmp -n \
	-e '/^#.!#/s/^#.!# \(.... [a-z][a-z_]*\)/\1/p';
mv lexicon.tmp lexicon.txt; rm -f lexicon_catalog.tmp lexicon_catalog.txt;

# Copy the filter to stdout. We're going to append sed commands to it after.
$SED -e 's|@sed@|'$SED'|' < ${LEXICON_FILTER_IN}

# Scan through the lexicon, transforming each line to something useful to Doxygen.
if echo x | $SED "/[[:<:]]x/d" 2>/dev/null; then
		WORDBL='[[:<:]]'; WORDBR='[[:>:]]';
	else
		WORDBL='\\<'; WORDBR='\\>';
	fi;
$SED < lexicon.txt -n -e "s|^\([a-z][a-z][a-z][a-z]\) \([a-z_-]*\)$|s,$WORDBL\2$WORDBR,@\1{\2},g|p" -e '$G;s/.*\n/b tidy/p';

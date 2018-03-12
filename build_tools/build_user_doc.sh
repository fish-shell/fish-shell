#!/bin/sh

# Usage: Doxyfile.user lexicon_filter
DOXYFILE=$1
LEXICON_FILTER=$2

(cat "${DOXYFILE}" ;\
 echo INPUT_FILTER="${LEXICON_FILTER}"; \
 echo PROJECT_NUMBER=${FISH_BUILD_VERSION} \
 | /usr/bin/env sed "s/-[a-z0-9-]*//") \
   | doxygen - && touch user_doc

(cd ./user_doc/html/ && \
 rm -f bc_s.png bdwn.png closed.png doc.png folder*.png ftv2*.png \
       nav*.png open.png splitbar.png sync_*.png tab*.* doxygen.* \
       dynsections.js jquery.js pages.html)

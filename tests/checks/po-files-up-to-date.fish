#RUN: fish=%fish %fish %s
#REQUIRES: msguniq --help
# Compiling in this test is too expensive.
# We need the gettext template extracted from the Rust sources passed in via env var,
# in order to pass it on.
#REQUIRES: test -e "$FISH_GETTEXT_EXTRACTION_FILE"

set -l dir (status dirname)

# Ensure that fish is in $PATH for the translation scripts.
set -lxp PATH (path dirname $fish)

# Using cargo in this test is expensive because $HOME is changed,
# so the entire toolchain needs to be downloaded and installed,
# followed by a clean build of the repo.
# The `--use-existing-template` argument allows using the pre-built version of the gettext template
# file.
$dir/../../build_tools/update_translations.fish --dry-run --use-existing-template=$FISH_GETTEXT_EXTRACTION_FILE
or exit 1

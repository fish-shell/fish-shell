# RUN: fish=%fish %fish %s
#REQUIRES: locale -a | grep 'de_DE'
#REQUIRES: locale -a | grep 'en_US'

set -l custom_gettext_var asdf

set -lx LANG de_DE.utf8

# Check if translations work.

$fish --not-a-real-flag
# CHECKERR: fish: --not-a-real-flag: unbekannte Option

set --show custom_gettext_var
# CHECK: $custom_gettext_var: Gesetzt in local Gültigkeit als nicht exportiert, mit 1 Elementen
# CHECK: $custom_gettext_var[1]: |asdf|

set -lx LANG en_US.utf8


# Changing the locale should result changing localizations.

$fish --not-a-real-flag
# CHECKERR: fish: --not-a-real-flag: unknown option

set --show custom_gettext_var
# CHECK: $custom_gettext_var: set in local scope, unexported, with 1 elements
# CHECK: $custom_gettext_var[1]: |asdf|

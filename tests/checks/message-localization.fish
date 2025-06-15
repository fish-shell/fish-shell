# RUN: fish=%fish %fish %s
#REQUIRES: command -v msgfmt
#REQUIRES: %fish -c 'status buildinfo | grep localize-messages'

set -l custom_gettext_var asdf

set -lx LANG de_DE.utf8

# Check if translations work.

$fish --not-a-real-flag
# CHECKERR: fish: --not-a-real-flag: unbekannte Option

set --show custom_gettext_var
# CHECK: $custom_gettext_var: Gesetzt in local Gültigkeit als nicht exportiert, mit 1 Elementen
# CHECK: $custom_gettext_var[1]: |asdf|

set -lx LANG en_US.utf8

# Changing the locale should result in different localizations.

$fish --not-a-real-flag
# CHECKERR: fish: --not-a-real-flag: unknown option

set --show custom_gettext_var
# CHECK: $custom_gettext_var: set in local scope, unexported, with 1 elements
# CHECK: $custom_gettext_var[1]: |asdf|

# Test un-exported, local scoping
begin
    set -l LANG de_DE.utf8
    echo (_ file)
    # CHECK: Datei
end
echo (_ file)
# CHECK: file

# Test LANGUAGE fallback
set -lx LANGUAGE pt de_DE

# This is available in Portuguese. (echo is used to get add a newline)
echo (_ file)
# CHECK: arquivo

# This is not available in Portuguese, but in German.
echo (_ "Control disassembler syntax highlighting style")
# CHECK: Steuert den Stil der Syntaxhervorhebung im Disassembler

# This is not available in any catalog.
echo (_ untranslatable)
# CHECK: untranslatable

# Check that C locale disables localization
begin
    set -l LANG C
    echo (_ file)
    # CHECK: file
end
echo (_ file)
# CHECK: arquivo
begin
    set -l LC_MESSAGES C
    echo (_ file)
    # CHECK: file
end
echo (_ file)
# CHECK: arquivo
begin
    set -l LC_ALL C
    echo (_ file)
    # CHECK: file
end
echo (_ file)
# CHECK: arquivo

# Check that all relevant locale variables are respected.
set --erase LANG
set --erase LC_MESSAGES
set --erase LC_ALL
set --erase LANGUAGE
echo (_ file)
# CHECK: file
begin
    set -l LANG fr_FR.utf8
    echo (_ file)
    # CHECK: fichier
    begin
        set -l LC_ALL de_DE.utf8
        echo (_ file)
        # CHECK: Datei
        set -g LC_MESSAGES pt_BR.utf8
        # Should still be German, since LC_ALL overrides LC_MESSAGES
        echo (_ file)
        # CHECK: Datei
    end
    # LC_ALL is no longer in scope, so now LC_MESSAGES should be used.
    echo (_ file)
    # CHECK: arquivo
end
# Now, only LC_MESSAGES should be in scope
echo (_ file)
# CHECK: arquivo
# LC_ALL should still override LC_MESSAGES
set -l LC_ALL de_DE.utf8
echo (_ file)
# CHECK: Datei

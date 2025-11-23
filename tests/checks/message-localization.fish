# RUN: fish=%fish %fish %s
#REQUIRES: command -v msgfmt
#REQUIRES: %fish -c 'status buildinfo | grep localize-messages'

set -l custom_gettext_var asdf

set -lx LANG de_DE.utf8

# Check if translations work.

$fish --not-a-real-flag
# CHECKERR: fish: --not-a-real-flag: unbekannte Option

set --show custom_gettext_var
# CHECK: $custom_gettext_var: Gesetzt in Geltungsbereich 'local', nicht exportiert, mit 1 Elementen
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
    set -l LANG POSIX
    echo (_ file)
    # CHECK: file
    set -l LANG C.UTF-8
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

# Check that empty vars are ignored
begin
    set -l LC_ALL
    echo (_ file)
    # CHECK: arquivo
end

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

# Check `status language` builtin
set --erase LANG
set --erase LC_MESSAGES
set --erase LC_ALL
set --erase LANGUAGE
status language
# CHECK: Active languages (default):
echo (_ file)
# CHECK: file

set -l LANGUAGE pt_BR de_DE
status language
# CHECK: Active languages (from variable LANGUAGE): pt_BR de
echo (_ file)
# CHECK: arquivo

# We have fr but not fr_FR. For the builtin command, only exact matches are allowed.
status language set fr_FR de pt_BR
# CHECKERR: No catalogs available for language specifiers: fr_FR
status language
# CHECK: Active languages (from command `status language set`): de pt_BR
echo (_ file)
# CHECK: Datei

set -l LANGUAGE zh_TW
status language
# CHECK: Active languages (from command `status language set`): de pt_BR
echo (_ file)
# CHECK: Datei

set -l LC_MESSAGES C
status language
# CHECK: Active languages (from command `status language set`): de pt_BR
echo (_ file)
# CHECK: Datei

status language unset
status language
# CHECK: Active languages (from variable LC_MESSAGES):
echo (_ file)
# CHECK: file

set --erase LC_MESSAGES
status language
# CHECK: Active languages (from variable LANGUAGE): zh_TW
echo (_ file)
# CHECK: 檔案

set --erase LANGUAGE
status language
# CHECK: Active languages (default):
echo (_ file)
# CHECK: file

# Check `status language set` warnings
status language set asdf
# CHECKERR: No catalogs available for language specifiers: asdf

# This will have to be changed if we add catalogs for languages used here.
status language set zh_HK it_IT
# CHECKERR: No catalogs available for language specifiers: zh_HK it_IT

status language set de de
# CHECKERR: Language specifiers appear repeatedly: de

status language set \xff quote\"
# CHECKERR: No catalogs available for language specifiers: \Xff 'quote"'

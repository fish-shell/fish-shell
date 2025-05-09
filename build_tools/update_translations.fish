#!/usr/bin/env fish

# Updates the files used for gettext translations.
# By default, the whole xgettext, msgmerge, msgfmt pipeline runs,
# which extracts the messages from the source files into messages.pot,
# updates the PO files for each language from that
# (changed line numbers, added messages, removed messages),
# and finally generates a machine-readable MO file for each language,
# which is stored in share/locale/$LANG/LC_MESSAGES/fish.mo (relative to the repo root).
#
# Use cases:
# For developers:
#   - Run with args `--no-mo` to update all PO files after making changes to Rust/fish
#     sources.
# For translators:
#   - Run with `--no-mo` first, to ensure that the strings you are translating are up to date.
#     Optionally, you can specify the language via `--lang=LANG`, where LANG is the
#     ISO 639-1 language code of the target language (eg de for German).

set build_script_dir (status dirname)

set --local extract
set --local po
set --local mo
set --local langs $build_script_dir/../po/*.po

argparse --exclusive 'no-mo,only-mo' 'no-mo' 'only-mo' 'lang=' -- $argv

if set --local --query _flag_no_mo
    set --local --erase mo
end
if set --local --query _flag_only_mo
    set --local --erase extract
    set --local --erase po
end
if set --local --query _flag_lang
    for lang_po_file in $langs
        if test (basename $lang_po_file .po) = $_flag_lang
            set --function language_found
            set --function langs $lang_po_file
            break
        end
    end
    if not set --function --query language_found
        echo "Language '$_flag_lang' not found. If you want to update translations for an existing language, use one of:"
        for lang_po_file in $langs
            printf ' %s' (basename $lang_po_file .po)
        end
        printf '\n\n'
        echo "If you want to add translations for a language which does not have any translations yet, navigate to the repository root in your shell and run:"
        echo " build_tools/fish_xgettext.fish && cp messages.pot po/cc.po"
        echo "where 'cc' is the two letter ISO 639-1 language code of the target language (eg de for German)."
        exit 1
    end
end

if set --local --query extract
    set --local extraction_script $build_script_dir/fish_xgettext.fish
    $extraction_script
end

for lang_po_file in $langs
    if set --local --query po
        msgmerge --update --no-fuzzy-matching --no-wrap --backup=none $lang_po_file messages.pot
    end
    if set --local --query mo
        set --local LOCALE_DIR $build_script_dir/../share/locale
        set --local OUT_DIR $LOCALE_DIR/(basename $lang_po_file .po)/LC_MESSAGES
        mkdir --parents $OUT_DIR
        msgfmt --check-format --output-file=$OUT_DIR/fish.mo $lang_po_file
    end
end

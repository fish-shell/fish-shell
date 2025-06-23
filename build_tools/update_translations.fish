#!/usr/bin/env fish

# Updates the files used for gettext translations.
# By default, the whole xgettext, msgmerge, msgfmt pipeline runs,
# which extracts the messages from the source files into $template_file,
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
#   - Specify the language you want to work on as an argument, which must be a file in the po/
#     directory. You can specify a language which does not have translations yet by specifying the
#     name of a file which does not yet exist. Make sure to follow the naming convention.
# For testing:
#   - Specify `--dry-run` to see if any updates to the PO files would by applied by this script.
#     If this flag is specified, the script will exit with an error if there are outstanding
#     changes, and will display the diff. Do not specify other flags if `--dry-run` is specified.
#
# Specify `--use-existing-template=FILE` to prevent running cargo for extracting an up-to-date
# version of the localized strings. This flag is intended for testing setups which make it
# inconvenient to run cargo here, but run it in an earlier step to ensure up-to-date values.
# This argument is passed on to the `fish_xgettext.fish` script and has no other uses.
# `FILE` must be the path to a gettext template file generated from our compilation process.
# It can be obtained by running:
#   set -l FILE (mktemp)
#   FISH_GETTEXT_EXTRACTION_FILE=$FILE cargo check

# The sort utility is locale-sensitive.
# Ensure that sorting output is consistent by setting LC_ALL here.
set -gx LC_ALL C.UTF-8

set -l build_tools (status dirname)
set -l template_file $build_tools/../po/template.po
set -l po_dir $build_tools/../po

set -l extract
set -l po
set -l mo

function cleanup_exit
    set -l exit_status $status

    if set -g --query tmpdir
        rm -r $tmpdir
    end

    exit $exit_status
end

argparse --exclusive 'no-mo,only-mo,dry-run' no-mo only-mo dry-run use-existing-template= -- $argv
or exit $status

# Make sure that the template file is not included in $po_files.
rm $template_file 2>/dev/null

if test -z $argv[1]
    # Update everything if not specified otherwise.
    set -g po_files $po_dir/*.po
else
    set -l po_dir_id (stat --format='%d:%i' -- $po_dir)
    for arg in $argv
        set -l arg_dir_id (stat --format='%d:%i' -- (dirname $arg))
        if test $po_dir_id != $arg_dir_id
            echo "Argument $arg is not a file in the directory $(realpath $po_dir)."
            echo "Non-option arguments must specify paths to files in this directory."
            echo ""
            echo "If you want to add a new language to the translations not the following:"
            echo "The filename must identify a language, with a two letter ISO 639-1 language code of the target language (e.g. 'pt' for Portuguese), and use the file extension '.po'."
            echo "Optionally, you can specify a regional variant (e.g. 'pt_BR')."
            echo "So valid filenames are of the shape 'll.po' or 'll_CC.po'."
            exit 1
        end
        if not basename $arg | grep -qE '^[a-z]{2}(_[A-Z]{2})?\.po$'
            echo "Filename does not match the expected format ('ll.po' or 'll_CC.po')."
            exit 1
        end
    end
    set -g po_files $argv
end

if set -l --query _flag_no_mo
    set -l --erase mo
end
if set -l --query _flag_only_mo
    set -l --erase extract
    set -l --erase po
end

if set -l --query extract
    set -l xgettext_args
    if set -l --query _flag_use_existing_template
        set xgettext_args --use-existing-template=$_flag_use_existing_template
    end
    $build_tools/fish_xgettext.fish $xgettext_args >$template_file
    or exit 1
end

# Protect from externally set $tmpdir leaking into this script.
set -g --erase tmpdir
if set -l --query _flag_dry_run
    # On a dry run, we do not modify po/ but write to a temporary directory instead and check if
    # there is a difference between po/ and the tmpdir after re-generating the PO files.
    set -g tmpdir (mktemp -d)

    # On a dry-run, we do not update the MO files.
    set -l --erase mo

    # Ensure tmpdir has the same initial state as the po dir.
    cp -r $po_dir/* $tmpdir
end

for po_file in $po_files
    if set -g --query tmpdir
        set po_file $tmpdir/(basename $po_file)
    end
    if set -l --query po
        if test -e $po_file
            msgmerge --update --no-fuzzy-matching --no-wrap --backup=none --quiet $po_file $template_file
            or cleanup_exit
        else
            cp $template_file $po_file
        end
    end
    if set -l --query mo
        set -l locale_dir $build_tools/../share/locale
        set -l out_dir $locale_dir/(basename $po_file .po)/LC_MESSAGES
        mkdir -p $out_dir
        msgfmt --check-format --output-file=$out_dir/fish.mo $po_file
    end
end

rm $template_file

if set -g --query tmpdir
    rm $tmpdir/template.po
    diff -ur $po_dir $tmpdir
    or cleanup_exit
end

cleanup_exit

# abort if `rename` is not installed
command -sq rename; or return

set -l rename_version (rename --version 2>/dev/null)
set -l version_status $status

if string match -q "*util-linux*" $rename_version[1]
    # util-linux version
    # https://www.mankier.com/1/rename
    complete -c rename -s s -l symlink -d 'Rename symlink target(s)'
    complete -c rename -s v -l verbose -n '! __fish_seen_argument -s n -l no-act' -d 'Show which files were renamed'
    complete -c rename -s v -l verbose -n '__fish_seen_argument -s n -l no-act' -d 'Show which files would be renamed'
    complete -c rename -s n -l no-act -d 'Make no changes'
    complete -c rename -s a -l all -d 'Replace all occurrences'
    complete -c rename -s l -l last -d 'Replace only the last occurrence'
    complete -c rename -s o -l no-overwrite -n '! __fish_seen_argument -s s -l symlink' -d "Don't overwrite existing files"
    complete -c rename -s o -l no-overwrite -n '__fish_seen_argument -s s -l symlink' -d "Don't overwrite symlink targets"
    complete -c rename -s i -l interactive -d 'Ask before overwriting'
    complete -c rename -s h -l help -d 'Print help text and exit'
    complete -c rename -s V -l version -d 'Print version and exit'
else if string match -q "*File::Rename*" $rename_version[1]
    # Perl library version
    # https://metacpan.org/release/File-Rename
    complete -c rename -s v -l verbose -d 'Print renamed files'
    complete -c rename -s 0 -l null -d 'Split on NUL when reading from stdin'
    complete -c rename -s n -l nono -d 'Only show what would be renamed'
    complete -c rename -s f -l force -d 'Overwrite existing files'
    complete -c rename -l path -l fullpath -n '! __fish_seen_argument -s d -l filename -l nopath -l nofullpath' -d 'Rename any directory component'
    complete -c rename -s d -l filename -l nopath -l nofullpath -n '! __fish_seen_argument -l path -l fullpath' -d 'Rename only filename component'
    complete -c rename -s h -l help -d 'Print synopsis and options'
    complete -c rename -s m -l man -d 'Print manual page'
    complete -c rename -s V -l version -d 'Show version number'
    complete -c rename -s u -l unicode -d 'Treat filenames as Unicode strings'
    complete -c rename -s e -x -d 'Perl expression'
    complete -c rename -s E -x -d 'Perl statement'
else if test "$version_status" = 2
    # `brew install rename`
    # http://plasmasturm.org/code/rename
    complete -c rename -s h -l help -d 'See a synopsis'
    complete -c rename -l man -d 'Browse the manpage'
    complete -c rename -s 0 -l null -d 'Split on NUL bytes'
    complete -c rename -s f -l force -d 'Rename over existing files'
    complete -c rename -s g -l glob -d 'Glob filename arguments'
    complete -c rename -s i -l interactive -d 'Confirm every action'
    complete -c rename -s k -l backwards -l reverse-order -d 'Process last file first'
    complete -c rename -s l -l symlink -n '! __fish_seen_argument -s L -l hardlink' -d 'Symlink instead of renaming'
    complete -c rename -s L -l hardlink -n '! __fish_seen_argument -s l -l symlink' -d 'Hardlink instead of renaming'
    complete -c rename -s M -l use -x -d 'Like perl -M'
    complete -c rename -s n -l dry-run -l just-print -d 'Only show what would be renamed'
    complete -c rename -s N -l counter-format -x -a '01 001 0001' -d 'Format and set $N per template'
    complete -c rename -s p -l mkpath -l make-dirs -d 'Create non-existent dirs'
    complete -c rename -l stdin -l no-stdin -d 'Always/never read list of files from STDIN'
    complete -c rename -s T -l transcode -x -d 'Transcode filenames'
    complete -c rename -s v -l verbose -d 'Print more information'
    complete -c rename -s a -l append -x -d 'Append STRING to each filename'
    complete -c rename -s A -l prepend -x -d 'Prepend STRING to each filename'
    complete -c rename -s c -l lower-case -d 'Convert to all lowercase'
    complete -c rename -s C -l upper-case -d 'Convert to all uppercase'
    complete -c rename -s e -l expr -x -d 'Perl expression'
    complete -c rename -s P -l pipe -d 'Pass filenames to external command'
    complete -c rename -s s -l subst -x -d 'Simple text substitution'
    complete -c rename -s S -l subst-all -x -d 'Like -s, but substitutes all matches'
    complete -c rename -s x -l remove-extension -d 'Remove extension'
    complete -c rename -s X -l keep-extension -d 'Save and remove extension'
    complete -c rename -s z -l sanitize -d 'Shortcut for --nows --noctrl --nometa --trim'
    complete -c rename -l camelcase -d 'Capitalise each word in the filename'
    complete -c rename -l urlesc -d 'Decode URL-escaped filenames'
    complete -c rename -l nows -d 'Replace all whitespace with underscores'
    complete -c rename -l rews -d 'Replace all underscores with whitespace'
    complete -c rename -l noctrl -d 'Replace all control chars with underscores'
    complete -c rename -l nometa -d 'Replace all shell meta-chars with underscores'
end

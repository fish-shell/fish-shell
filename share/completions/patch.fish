#
# Completions for GNU patch
# TODO: POSIX patch
#

complete -c patch -s b -l backup -d "Back up the original contents of each file"
complete -c patch -l backup-if-mismatch -d "Back up files if patch doesn't match exactly"
complete -c patch -l no-backup-if-mismatch -d "Don't back up for mismatching patches"
complete -c patch -s B -l prefix -x -d "Prepend PREFIX to backup filenames"
complete -c patch -s c -l context -d "Interpret patch as context diff"
complete -c patch -s d -l directory -xa '(__fish_complete_directories (commandline -ct))' -d "Change directory to DIR first"
complete -c patch -s D -l ifdef -x -d "Make merged if-then-else output using NAME"
complete -c patch -l dry-run -d "Don't change files; just print what would happen"
complete -c patch -s e -l ed -d "Interpret patch as an 'ed' script"
complete -c patch -s E -l remove-empty-files -d "Remove output files empty after patching"
complete -c patch -s f -l force -d "Like -t, but ignore bad-Prereq patches, assume unreversed"
complete -c patch -s F -l fuzz -x -d "Number of LINES for inexact 'fuzzy' matching" -a "(seq 0 9){\tfuzz lines}"
complete -c patch -s g -l get -x -d "Get files from RCS etc. if positive; ask if negative" -a '(seq -1 1){\t\n}'
complete -c patch -l help -f -d "Display help"
complete -c patch -s i -l input -x -d "Read patch from FILE instead of stdin" -k -a "( __fish_complete_suffix .patch .diff)"
complete -c patch -s l -l ignore-whitespace -d "Ignore whitespace changes between patch & input"
complete -c patch -s n -l normal -d "Interpret patch as normal diff"
complete -c patch -s N -l forward -d "Ignore patches that seem reversed or already applied"
complete -c patch -s o -l output -r -d "Output patched files to FILE"
complete -c patch -s p -l strip -x -d "Strip NUM path components from filenames" -a "(seq 0 9){\t\n}"
complete -c patch -l posix -d "Conform to the POSIX standard"
complete -c patch -l quoting-style -x -d "Output file names using quoting style" -a "literal shell shell-always c escape"
complete -c patch -s r -l reject-file -r -d "Output rejects to FILE"
complete -c patch -s R -l reverse -d "Assume patches were created with old/new files swapped"
complete -c patch -s s -l silent -l quiet -d "Work silently unless an error occurs"
complete -c patch -s t -l batch -d "Ask no questions; skip bad-Prereq patches; assume reversed"
complete -c patch -s T -l set-time -d "Set times of patched files assuming diff uses local time"
complete -c patch -s u -l unified -d "Interpret patch file as a unified diff"
complete -c patch -s v -l version -d "Display version"
complete -c patch -s V -l version-control -x -d "Use method to determine backup file names" -a "simple numbered existing"
complete -c patch -l verbose -d "Output extra information about work being done"
complete -c patch -s x -l debug -x -d "Set internal debugging flags"
complete -c patch -s Y -l basename-prefix -x -d "Prepend PREFIX to backup file basenames"
complete -c patch -s z -l suffix -x -d "Append SUFFIX to backup file name"
complete -c patch -s Z -l set-utc -d "Set times of patched files assuming diff uses UTC"

# No effect on POSIX systems that don't use O_BINARY/O_TEXT
uname -s | string match -q CYGWIN\*
and complete -c patch -l binary -d "Read & write data in binary mode"

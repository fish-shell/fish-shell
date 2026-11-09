# Completion for od (https://www.gnu.org/software/coreutils/od)

complete -c od -s A -l address-radix -xa "
    d\tDecimals
    o\tOctals
    x\tHex
    n\tNone
    " -d "output format for file offsets"
complete -c od -l endian -xa "big little" -d "swap input bytes according the specified order"
complete -c od -s j -l skip-bytes -x -d "skip BYTES input bytes first"
complete -c od -s N -l read-bytes -x -d "limit dump to BYTES input bytes"
complete -c od -s S -l strings -f -d "show only NUL terminated strings of at least BYTES (default 3) printable characters"
complete -c od -s t -l format -xa "
    a\t'select named characters, ignoring high-order bit'
    o1\t'select octal bytes'
    c\t'select printable characters or backslash escapes'
    u2\t'select unsigned decimal 2-byte units'
    fF\t'select floats'
    dI\t'select decimal ints'
    dL\t'select decimal longs'
    o2\t'select octal 2-byte units'
    d2\t'select decimal 2-byte units'
    x2\t'select hexadecimal 2-byte units'
    " -d "select output format or formats"
complete -c od -s b -d "same as -t o1, select octal bytes"
complete -c od -s c -d "same as -t c,  select printable characters or backslash escapes"
complete -c od -s d -d "same as -t u2, select unsigned decimal 2-byte units"
complete -c od -s f -d "same as -t fF, select floats"
complete -c od -s i -d "same as -t dI, select decimal ints"
complete -c od -s l -d "same as -t dL, select decimal longs"
complete -c od -s o -d "same as -t o2, select octal 2-byte units"
complete -c od -s s -d "same as -t d2, select decimal 2-byte units"
complete -c od -s x -d "same as -t x2, select hexadecimal 2-byte units"
complete -c od -s a -d "same as -t a,  select named characters, ignoring high-order bit"
complete -c od -s v -l output-duplicates -d "do not use * to mark line suppression"
complete -c od -s w -l width -f -d "output BYTES bytes per output line; 32 is implied when BYTES is not specified"
complete -c od -l traditional -d "accept arguments in third form above"
complete -c od -l help -d "display this help and exit"
complete -c od -l version -d "output version information and exit"

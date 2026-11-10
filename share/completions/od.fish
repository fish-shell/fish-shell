# Completion for od (https://www.gnu.org/software/coreutils/od)

complete -c od -s A -l address-radix -xa "
    d\tDecimals
    o\tOctals
    x\tHex
    n\tNone
    " -d "output format for file offsets"
complete -c od -l endian -xa "big little" -d "swap input bytes according the specified order"
complete -c od -s j -l skip-bytes -x -d "number of input bytes to skip"
complete -c od -s N -l read-bytes -x -d "number of input bytes to limit to"
complete -c od -s S -l strings -f -d "show only NUL terminated strings of at least this many printable characters"
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
complete -c od -s b -d "select octal bytes"
complete -c od -s c -d "select printable characters or backslash escapes"
complete -c od -s d -d "select unsigned decimal 2-byte units"
complete -c od -s f -d "select floats"
complete -c od -s i -d "select decimal ints"
complete -c od -s l -d "select decimal longs"
complete -c od -s o -d "select octal 2-byte units"
complete -c od -s s -d "select decimal 2-byte units"
complete -c od -s x -d "select hexadecimal 2-byte units"
complete -c od -s a -d "select named characters, ignoring high-order bit"
complete -c od -s v -l output-duplicates -d "do not use * to mark line suppression"
complete -c od -s w -l width -f -d "number of bytes per output line"
complete -c od -l traditional -d "accept arguments in traditional format"
complete -c od -l help -d "display this help and exit"
complete -c od -l version -d "output version information and exit"

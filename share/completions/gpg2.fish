#
# Completions for the gpg2 command.
#
# This program accepts an rather large number of switches. It allows
# you to do things like changing what file descriptor errors should be
# written to, to make gpg2 use a different locale than the one
# specified in the environment or to specify an alternative home
# directory.

# Switches related to debugging, switches whose use is not
# recommended, switches whose behaviour is as of yet undefined,
# switches for experimental features, switches to make gpg2 compliant
# to legacy pgp-versions, dos-specific switches, switches meant for
# the options file and deprecated or obsolete switches have all been
# removed. The remaining list of completions is still quite
# impressive.

#
# Various functions used for dynamic completions
#

function __fish_complete_gpg2_user_id -d "Complete using gpg2 user ids"
    # gpg2 doesn't seem to like it when you use the whole key name as a
    # completion, so we skip the <EMAIL> part and use it as a
    # description.
    # It also replaces colons with \x3a
    gpg2 --list-keys --with-colon | cut -d : -f 10 | sed -ne 's/\\\x3a/:/g' -e 's/\(.*\) <\(.*\)>/\1'\t'\2/p'
end

function __fish_complete_gpg2_key_id -d 'Complete using gpg2 key ids'
    # Use user id as description
    set -l keyid
    gpg2 --list-keys --with-colons | while read garbage
        switch $garbage
            # Extract user id
            case "uid*"
                echo $garbage | cut -d ":" -f 10 | sed -e "s/\\\x3a/:/g" | read uid
                printf "%s\t%s\n" $keyid $uid
            # Extract key fingerprint (no subkeys)
            case "pub*"
                echo $garbage | cut -d ":" -f 5 | read keyid
        end
    end
end

function __fish_print_gpg2_algo -d "Complete using all algorithms of the type specified in argv[1] supported by gpg. argv[1] is a regexp"
    # Set a known locale, so that the output format of 'gpg2 --version'
    # is at least somewhat predictable. The locale will automatically
    # expire when the function goes out of scope, and the original locale
    # will take effect again.
    set -lx LC_ALL C

    # sed script explained:
    # in the line that matches "$argv:"
    #   define label 'loop'
    #   if the line ends with a ','
    #       add next line to buffer
    #       transliterate '\n' with ' '
    #       goto loop
    #   remove everything until the first ':' of the line
    #   remove all blanks
    #   transliterate ',' with '\n' (macOS apparently doesn't like '\n' on RHS of the s-command)
    #   print result
    #   sed script must be formatted with newlines instead of semicolons to be compatible
    #   with BSD sed (eg, on macOS)
    gpg2 --version | sed -ne "/$argv:/"'{:loop
    /,$/{N; y!\n! !
    b loop
    }
    s!^[^:]*:!!
    s![ ]*!!g
    y!,!\n!
    p
    }'
end

__fish_complete_gpg gpg2

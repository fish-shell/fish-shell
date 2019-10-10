# Helper function for contextual autocompletion of GPG crypto algorithm options

function __fish_print_gpg_algo -d "Complete using all algorithms of the type specified in argv[2] supported by gpg. argv[2] is a regexp" -a __fish_complete_gpg_command
    # Set a known locale, so that the output format of 'gpg --version'
    # is at least somewhat predictable. The locale will automatically
    # expire when the function goes out of scope, and the original locale
    # will take effect again.
    set -lx LC_ALL C

    # sed script explained:
    # in the line that matches "$argv:"
    # 	define label 'loop'
    # 	if the line ends with a ','
    # 		add next line to buffer
    #		transliterate '\n' with ' '
    #		goto loop
    # 	remove everything until the first ':' of the line
    # 	remove all blanks
    # 	transliterate ',' with '\n' (OSX apparently doesn't like '\n' on RHS of the s-command)
    # 	print result
    $__fish_complete_gpg_command --version | sed -ne "/$argv[2]:/"'{:loop
    /,$/{N; y!\n! !
    b loop
    }
    s!^[^:]*:!!
    s![ ]*!!g
    y!,!\n!
    p
    }'
end

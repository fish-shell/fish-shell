# Helper function for completions that need to enumerate Linux modules
function __ghoti_print_modules
    find /lib/modules/(uname -r)/{kernel,misc} -type f 2>/dev/null | sed -e 's$/.*/\([^/.]*\).*$\1$'
end

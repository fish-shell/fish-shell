function __fish_print_make_targets --argument directory
    # Since we filter based on localized text, we need to ensure the
    # text will be using the correct locale.
    set -lx LC_ALL C

    if test -z "$directory"
        set directory '.'
    end

    set -l bsd_make
    if make -C $directory -pn >/dev/null ^/dev/null
        set bsd_make 0
    else
        set bsd_make 1
    end

    for file in $directory/{GNUmakefile,Makefile,makefile}
        if test -f $file
            if test "$bsd_make" = 0
                make -C $directory -prRn | awk -v RS= -F: '/^# Files/,/^# Finished Make data base/ {if ($1 !~ "^[#.]") {print $1}}' ^/dev/null
            else
                make -C $directory -d g1 -rn >/dev/null ^| awk -F, '/^#\*\*\* Input graph:/,/^$/ {if ($1 !~ "^#... ") {gsub(/# /,"",$1); print $1}}' ^/dev/null
            end
            break
        end
    end
end


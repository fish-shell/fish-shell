function __fish_print_make_targets --argument directory
    # Since we filter based on localized text, we need to ensure the
    # text will be using the correct locale.
    set -lx LC_ALL C

    if test -z "$directory"
        set directory '.'
    end

    for file in $directory/{GNUmakefile,Makefile,makefile}
        if test -f $file
            make -C $directory -prRn | awk -v RS= -F: '/^# Files/,/^# Finished Make data base/ {if ($1 !~ "^[#.]") {print $1}}' ^/dev/null
            break
        end
    end
end


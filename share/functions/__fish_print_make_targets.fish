function __fish_print_make_targets --argument directory
    if test -z "$directory"
        set directory '.'
    end

    for file in $directory/{GNUmakefile,Makefile,makefile}
        if test -f $file
            make -C $directory -prRn | awk -v RS= -F: '/^# File/,/^# Finished Make data base/ {if ($1 !~ "^[#.]") {print $1}}' ^/dev/null
            break
        end
    end
end


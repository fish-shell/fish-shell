function __fish_print_commands --description "Print a list of documented fish commands"
    if test -d $__fish_datadir/man/man1/
        for file in $__fish_datadir/man/man1/**.1*
            string replace -r '.*/' '' -- $file | string replace -r '.1(.gz)?$' ''
        end
    end
end

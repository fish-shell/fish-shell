function __fish_print_commands --description "Print a list of documented fish commands"
    if test -d $__fish_data_dir/man/man1/
        for file in $__fish_data_dir/man/man1/**.1*
            string replace -r '.*/' '' -- $file |
                string replace -r '.1(.gz)?$' '' |
                string match -rv '^fish-(?:changelog|completions|doc|tutorial|faq|for-bash-users|interactive|language|releasenotes)$'
        end
    end
end

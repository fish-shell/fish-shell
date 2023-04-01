function __ghoti_print_commands --description "Print a list of documented ghoti commands"
    if test -d $__ghoti_data_dir/man/man1/
        for file in $__ghoti_data_dir/man/man1/**.1*
            string replace -r '.*/' '' -- $file |
                string replace -r '.1(.gz)?$' '' |
                string match -rv '^ghoti-(?:changelog|completions|doc|tutorial|faq|for-bash-users|interactive|language|releasenotes)$'
        end
    end
end

function dirs --description 'Print directory stack'
        # process options
        if count $argv >/dev/null
                switch $argv[1]
                        case -c
                                # clear directory stack
                                set -e -g dirstack
				return 0
		end
        end

        # replace $HOME with ~
        echo -n (echo (command pwd) | sed -e "s|^$HOME|~|")"  "
        for i in $dirstack
                echo -n (echo $i | sed -e "s|^$HOME|~|")"  "
        end
        echo
end

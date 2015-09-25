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
        if test (count $dirstack) -gt 0
                for i in (seq (count $dirstack))
                        echo -n +$i:(echo $dirstack[$i] | sed -e "s|^$HOME|~|")"  "
                end
        end
        echo
end

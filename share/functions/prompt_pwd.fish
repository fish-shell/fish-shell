function prompt_pwd --description 'short CWD for the prompt'
    set -l options h/help d/dir-length= D/full-length-dirs=
    argparse -n prompt_pwd $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help prompt_pwd
        return 0
    end

    set -q argv[1]
    or set argv $PWD

    set -ql _flag_d
    and set -l fish_prompt_pwd_dir_length $_flag_d

    set -q fish_prompt_pwd_dir_length
    or set -l fish_prompt_pwd_dir_length 1

    set -l fulldirs 0
    set -ql _flag_D
    and set -l fish_prompt_pwd_full_dirs $_flag_D

    set -q fish_prompt_pwd_full_dirs
    or set -l fish_prompt_pwd_full_dirs 1

    for path in $argv
        # Replace $HOME with "~"
        set -l realhome (string escape --style=regex -- ~)
        set -l tmp (string replace -r '^'"$realhome"'($|/)' '~$1' $path)

        if test "$fish_prompt_pwd_dir_length" -eq 0
            echo $tmp
        else
            # Shorten to at most $fish_prompt_pwd_dir_length characters per directory
            # with full-length-dirs components left at full length.
            set -l full
            if test $fish_prompt_pwd_full_dirs -gt 0
                set -l all (string split -m (math $fish_prompt_pwd_full_dirs - 1) -r / $tmp)
                set tmp $all[1]
                set full $all[2..]
            else if test $fish_prompt_pwd_full_dirs -eq 0
                # 0 means not even the last component is kept
                string replace -ar '(\.?[^/]{'"$fish_prompt_pwd_dir_length"'})[^/]*' '$1' $tmp
                continue
            end

            string join / -- (string replace -ar -- '(\.?[^/]{'"$fish_prompt_pwd_dir_length"'})[^/]*/' '$1/' $tmp) $full
        end
    end
end

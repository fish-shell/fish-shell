function __fish_istioctl_perform_completion

    # Extract all args except the last one
    set -l args (commandline -opc)
    # Extract the last arg and escape it in case it is a space
    set -l lastArg (string escape -- (commandline -ct))
    set -l requestComp "$args[1] __complete $args[2..-1] $lastArg"
    set -l results (eval $requestComp 2> /dev/null)

    # Some programs may output extra empty lines after the directive.
    # Let's ignore them or else it will break completion.
    # Ref: https://github.com/spf13/cobra/issues/1279
    set results (string trim -- $results | string match -r '.+')

    set -l comps $results[1..-2]
    set -l directiveLine $results[-1]

    # For Fish, when completing a flag with an = (e.g., <program> -n=<TAB>)
    # completions must be prefixed with the flag
    set -l flagPrefix (string match -r -- '-.*=' "$lastArg")

    for comp in $comps
        printf "%s%s\n" "$flagPrefix" "$comp"
    end
    printf "%s\n" "$directiveLine"
end

function __fish_istioctl_prepare_completions
    set -l results (__fish_istioctl_perform_completion)

    if test -z "$results"
        return 1
    end

    set -l directive (string sub --start 2 $results[-1])
    set -l __istioctl_comp_results $results[1..-2]
    set -l shellCompDirectiveError 1
    set -l shellCompDirectiveNoSpace 2
    set -l shellCompDirectiveNoFileComp 4
    set -l shellCompDirectiveFilterFileExt 8
    set -l shellCompDirectiveFilterDirs 16

    if test -z "$directive"
        set directive 0
    end

    set -l compErr (math (math --scale 0 $directive / $shellCompDirectiveError) % 2)
    if test $compErr -eq 1
        return 1
    end

    set -l filefilter (math (math --scale 0 $directive / $shellCompDirectiveFilterFileExt) % 2)
    set -l dirfilter (math (math --scale 0 $directive / $shellCompDirectiveFilterDirs) % 2)
    if test $filefilter -eq 1; or test $dirfilter -eq 1
        return 1
    end

    set -l nospace (math (math --scale 0 $directive / $shellCompDirectiveNoSpace) % 2)
    set -l nofiles (math (math --scale 0 $directive / $shellCompDirectiveNoFileComp) % 2)

    # If we want to prevent a space, or if file completion is NOT disabled,
    # we need to count the number of valid completions.
    # To do so, we will filter on prefix as the completions we have received
    # may not already be filtered so as to allow fish to match on different
    # criteria than the prefix.
    if test $nospace -ne 0; or test $nofiles -eq 0
        # Important not to quote the variable for count to work
        set -l numComps (count $__istioctl_comp_results)
        if test $numComps -eq 1; and test $nospace -ne 0
            # We must first split on \t to get rid of the descriptions to be
            # able to check what the actual completion will be.
            # We don't need descriptions anyway since there is only a single
            # real completion which the shell will expand immediately.
            set -l split (string split --max 1 \t $__istioctl_comp_results[1])

            # Fish won't add a space if the completion ends with any
            # of the following characters: @=/:.,
            set -l lastChar (string sub -s -1 -- $split)
            if not string match -r -q "[@=/:.,]" -- "$lastChar"
                # In other cases, to support the "nospace" directive we trick the shell
                # by outputting an extra, longer completion.
                set -l __istioctl_comp_results $split[1] $split[1].
            end
        end
    end
    
    for result in $__istioctl_comp_results
        printf "%s\n" "$result"
    end
end

set -l file_subcommands analyze manifest

complete -c istioctl -n "not __fish_seen_subcommand_from $file_subcommands" -f -a '(__fish_istioctl_prepare_completions)'
complete -c istioctl -n "__fish_seen_subcommand_from $file_subcommands" -rF

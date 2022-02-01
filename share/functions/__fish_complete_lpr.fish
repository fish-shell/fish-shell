function __fish_complete_lpr -d 'Complete lpr common options' --argument-names cmd
    complete -c $cmd -s E -d 'Forces encryption when connecting to the server'
    complete -c $cmd -s U -d 'Specifies an alternate username' -xa '(__fish_complete_users)'

    switch $cmd
        case lpr lpq lprm
            complete -c $cmd -s P -d 'Specifies an alternate printer or class name' -xa '(__fish_print_lpr_printers)'
    end

    switch $cmd
        case lpq cancel
            complete -c $cmd -s a -d 'Apply command to all printers'
    end

    switch $cmd
        case lpq cancel lpmove lpstat lprm lpoptions lp reject accept cupsaccept cupsreject cupsenable cupsdisable
            complete -c $cmd -s h -d 'Specifies an alternate server' -xa '(__fish_print_hostnames)'
    end

    switch $cmd
        case lp lpr
            complete -c $cmd -s m -d 'Send an email on job completion'

            complete -c $cmd -s o -xa landscape -d 'Landscape mode'
            complete -c $cmd -s o -xa "media=a4 media=letter media=legal" -d 'Media size'
            complete -c $cmd -s o -xa page-ranges= -d 'Page ranges'
            complete -c $cmd -s o -xa orientation-requested= -d 'Choose orientation (4-landscape)'
            complete -c $cmd -s o -xa 'sides-one-sided two-sided-long-edge two-sided-short-edge' -d 'Choose between one/two sided modes'
            complete -c $cmd -s o -xa fitplot -d 'Scale the print file to fit on the page'
            complete -c $cmd -s o -xa 'number-up=2 number-up=4 number-up=6 number-up=9' -d 'Print multiple document pages on each output page'
            complete -c $cmd -s o -xa 'scaling=' -d 'Scale image files to use up to number percent of the page'
            complete -c $cmd -s o -xa 'cpi=' -d 'Set the number of characters per inch to use'
            complete -c $cmd -s o -xa 'lpi=' -d 'Set the number of lines per inch to use'
            complete -c $cmd -s o -xa 'page-bottom= page-left= page-right= page-top=' -d 'Set the page margins when printing text files'
            # this must be last
            complete -c $cmd -s o -d 'Sets a job option' -xa '(__fish_complete_lpr_option)'
    end
end

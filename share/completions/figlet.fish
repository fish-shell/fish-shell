function __fish_print_figlet_fonts
    set -l dir (figlet -I 2)
    set -l files $dir/*.flf $dir/*.tlf

    printf '%s\tFont\n' (string replace -r '.*/([^/]+)\.[ft]lf' '$1' -- $files)
end

complete -c figlet -s f -d "Select font" -x -a "(__fish_print_figlet_fonts)"
complete -c figlet -s d -d "Change font directory"
complete -c figlet -s c -d "Center output horizontally"
complete -c figlet -s l -d "Make output flush-left"
complete -c figlet -s r -d "Make output flush-right"
complete -c figlet -s x -d "Set justification according to text direction"
complete -c figlet -s t -d "Set output to terminal width"
complete -c figlet -s w -d "Set output width"
complete -c figlet -s p -d "Put FIGlet into `paragraph mode`"
complete -c figlet -s n -d "Put FIGlet back to normal"
complete -c figlet -s D -d "Switch to German (ISO 646-DE) character set"
complete -c figlet -s E -d "Turns off German character set processing"
complete -c figlet -s C -d "Add given control file" -k -a "(__fish_complete_suffix .flc)" -x
complete -c figlet -s N -d "Clear control file list"
complete -c figlet -s s -d "Cause `smushing`"
complete -c figlet -s S -d "Cause `smushing`"
complete -c figlet -s k -d "Cause `kerning`"
complete -c figlet -s W -d "Makes FIGlet all FIGcharacters at full width"
complete -c figlet -s o -d "Cause `overlapped`"
complete -c figlet -s m -d "Specify layout mode between 1 and 63"
complete -c figlet -s v -d "Print version and copyright"
complete -c figlet -s I -d "Print information given infocode" -x -a "0\tVersion\ and\ copyright 1\tVersion\ \(integer\) 2\tDefault\ font\ directory 3\tFont 4\tOutput\ width 5\tSupported\ font\ format"
complete -c figlet -s L -d "Print left-to-right"
complete -c figlet -s R -d "Print right-to-left"
complete -c figlet -s X -d "Print with default text direction"

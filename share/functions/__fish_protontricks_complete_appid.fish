# localization: skip(private)
function __fish_protontricks_complete_appid
    # '-L' is a newer flag that lists all games without a graphical prompt
    # if multiple Steam installations exist
    set output "$(protontricks -L 2>/dev/null || protontricks -l 2>/dev/null)"
    echo $output |
        string match --regex '.*\(\d+\)' |
        string replace --regex '(.*) \((\d+)\)' '$2\t$1'
end

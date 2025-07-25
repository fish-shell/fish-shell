function __fish_protontricks_complete_appid
    protontricks -l |
        string match --regex '.*\(\d+\)' |
        string replace --regex '(.*) \((\d+)\)' '$2\t$1'
end

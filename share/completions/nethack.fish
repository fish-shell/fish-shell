function __fish_complete_nethack
    set -l roles Archeologist Barbarian Cave{man,woman} Healer Knight Monk \
        Priest{,ess} Rogue Ranger Samurai Tourist Valkyrie Wizard

    set -l races Human Elf Dwarf Gnome Orc

    for r in $$argv[1]
        printf "%s\t%s\n" (string sub -l 3 $r) $r
    end
end

complete -c nethack -s d -ra "(__fish_complete_directories)" -d 'Specify a playground directory'
complete -c nethack -s n -d 'Suppress printing any news from the game administrator'
complete -c nethack -s p -x -a "(__fish_complete_nethack roles)" -d 'Specify profession'
complete -c nethack -s r -x -a "(__fish_complete_nethack races)" -d 'Specify race'
complete -c nethack -s D -d 'Start in debugging (wizard) mode'
complete -c nethack -s X -d 'Start in discovery mode'
complete -c nethack -s u -x -d 'Specify player name'
complete -c nethack -o dec -d 'Use DEC graphics'
complete -c nethack -o ibm -d 'Use IBM graphics'
complete -c nethack -l version -d 'Show version information'
complete -c nethack -l version:paste -d 'Show version information & copy it to paste buffer'
complete -c nethack -s s -d 'Print the list of your scores'
complete -c nethack -s v -d 'Print all versions present in the score file'

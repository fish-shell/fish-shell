function __fish_print_console_keymaps
    # The path(s) used may differ by linux distribution, and are compiled into
    # loadkeys, which doesn't provide a way to check which paths are searched
    # or which keymaps it can find. localectl can list keymaps, but is part of
    # systemd, which we shouldn't depend on.

    set -l dirs /usr/share/kbd/keymaps /usr/share/keymaps /usr/lib/kbd/keymaps /lib/kbd/keymaps /usr/src/linux/drivers

    path filter -f $dirs/** | string replace -rf '.*/(.*)\.k?map(|\..*)$' '$1'
end

complete -fc loadkeys -a "(__fish_print_console_keymaps)"

complete -c loadkeys -s C -l console -d "Console device to use" -x -a "(path filter --type=char /dev/tty*)"
complete -c loadkeys -s a -l ascii -d "Force conversion to ASCII"
complete -c loadkeys -s b -l bkeymap -d "Output binary keymap"
complete -c loadkeys -s c -l clearcompose -d "Clear kernel compose table"
complete -c loadkeys -s d -l default -d "Load default keymap"
complete -c loadkeys -s m -l mktable -d "Output a 'defkeymap.c'"
complete -c loadkeys -s p -l parse -d "Only search and parse keymap"
complete -c loadkeys -s s -l clearstrings -d "Clear kernel string table"
complete -c loadkeys -s u -l unicode -d "Force conversion to Unicode"
complete -c loadkeys -s q -l quiet -d "Supress all normal output"
complete -c loadkeys -s v -l verbose -d "Be more verbose"
complete -c loadkeys -s V -l version -d "Print version number"
complete -c loadkeys -s h -l help -d "Print help"

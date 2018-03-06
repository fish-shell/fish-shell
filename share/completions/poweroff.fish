
# magic completion safety check (do not remove this comment)
if not type -q poweroff
    exit
end
complete -c poweroff -l help -d "Show help"
complete -c poweroff -l halt -d "Halt the machine"
complete -c poweroff -l poweroff -s p -d "Switch off the machine"
complete -c poweroff -l reboot -d "Reboot the machine"
complete -c poweroff -l force -s f -d "Force immediate halt/power-off/reboot"
complete -c poweroff -l wtmp-only -s w -d "Just write wtmp record"
complete -c poweroff -l no-wtmp -s d -d "Don't write wtmp record"
complete -c poweroff -l no-wall -d "Don't send wall message"

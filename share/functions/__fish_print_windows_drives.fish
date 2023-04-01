function __ghoti_print_windows_drives --description 'Print Windows drives'
    wmic logicaldisk get name | tail +2
end

function __fish_print_windows_drives --description 'Print Windows drives'
    wmic logicaldisk get name | tail +2
end

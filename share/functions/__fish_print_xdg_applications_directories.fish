# localization: skip(private)
function __fish_print_xdg_applications_directories --description 'Print directories where desktop files are stored'
    for path in (__fish_print_xdg_data_directories)/applications
        if test -d $path
            echo $path
        end
    end
end

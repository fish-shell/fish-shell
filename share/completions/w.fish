if __fish_supports_version w
    complete -c w -s h -d "Dont print header"
    complete -c w -s u -d "Ignore username for time calculations"
    complete -c w -s s -d "Short format"
    complete -c w -s f -d "Toggle printing of remote hostname"
    complete -c w -s V -d "Display version and exit"
else
    complete -c w -s h -d "Dont print header"
    complete -c w -s i -d "Print idle time"
    complete -c w -s n -d "Do not resolve host names"
end
complete -c w -x -a "(__fish_complete_users)" -d Username

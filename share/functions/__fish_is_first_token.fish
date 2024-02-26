function __fish_is_first_token -d 'Test if no non-switch argument has been specified yet'
    __fish_is_nth_token 1
end

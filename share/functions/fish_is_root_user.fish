# To know if the user is root. Used by several prompts to display something
# else if the user is root.

function fish_is_root_user --description "Check if the user is root"
    if contains -- $USER root toor Administrator
        return 0
    end

    return 1
end

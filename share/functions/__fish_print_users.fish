# This should be used where you want user names without a description. If you also want
# a description, such as when getting a list of users for a completion, you probably want
# __fish_complete_users.
function __fish_print_users --description "Print a list of local users"
    # Leave the heavy lifting to __fish_complete_users but strip the descriptions.
    __fish_complete_users | string replace -r '^([^\t]*)\t.*' '$1'
end

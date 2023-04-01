# This should be used where you want group names without a description. If you also want
# a description, such as when getting a list of groups for a completion, you probably want
# __ghoti_complete_groups.
function __ghoti_print_groups --description "Print a list of local groups"
    # Leave the heavy lifting to __ghoti_complete_groups but strip the descriptions.
    __ghoti_complete_groups | string replace -r '^([^\t]*)\t.*' '$1'
end

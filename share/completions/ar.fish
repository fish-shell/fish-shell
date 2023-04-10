function __single
    complete -f -c ar -n __fish_use_subcommand -a $argv[1] -d $argv[2] # no dash
end

__single d "Delete modules from the archive."
__single m "move members in an archive."
__single p "Print the specified members of the archive, to the standard output file."
__single q "Quick append; Historically, add the files member... to the end of archive, without checking for replacement."
__single r "Insert the files member... into archive (with replacement)."
__single s "Add an index to the archive, or update it if it already exists."
__single t "Display a table listing the contents of archive, or those of the files listed in member."
__single x "Extract members (named member) from the archive."

functions -e __single

# TODO add mod
# A number of modifiers (mod) may immediately follow the p keyletter, to specify variations on an operation's behavior:
# add dash
# command format
# ar [emulation options] [-]{dmpqrstx}[abcDfilMNoOPsSTuvV] [--plugin <name>] [member-name] [count] archive-file file

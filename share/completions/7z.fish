# Completions for p7zip

# Commands
complete -f -c 7z -n __fish_use_subcommand -a a -d Add
complete -f -c 7z -n __fish_use_subcommand -a b -d Benchmark
complete -f -c 7z -n __fish_use_subcommand -a d -d Delete
complete -f -c 7z -n __fish_use_subcommand -a e -d Extract
complete -f -c 7z -n __fish_use_subcommand -a h -d Hash
complete -f -c 7z -n __fish_use_subcommand -a i -d "Show information about supported formats"
complete -f -c 7z -n __fish_use_subcommand -a l -d List
complete -f -c 7z -n __fish_use_subcommand -a rn -d Rename
complete -f -c 7z -n __fish_use_subcommand -a t -d Test
complete -f -c 7z -n __fish_use_subcommand -a u -d Update
complete -f -c 7z -n __fish_use_subcommand -a x -d "Extract with full paths"

# Switches
complete -c 7z -n "__fish_seen_subcommand_from e l t x" -o ai -d "Include archives"
complete -c 7z -n "__fish_seen_subcommand_from e l t x" -o an -d "Disable archive_name field"
complete -c 7z -n "__fish_seen_subcommand_from e x" -o ao -d "Overwrite mode"
complete -c 7z -n "__fish_seen_subcommand_from e l t x" -o ax -d "Exclude archives"
complete -c 7z -n "__fish_seen_subcommand_from a d e u x" -o bb -d "Set output log level"
complete -c 7z -o bd -d "Disable progress indicator"
complete -c 7z -n "__fish_seen_subcommand_from a d e h l u x" -o bs -d "Set output stream"
complete -c 7z -o bt -d "Show execution time statistics"
complete -c 7z -n "__fish_seen_subcommand_from a d e h l rn t u x" -o i -d "Include filenames"
complete -c 7z -n "__fish_seen_subcommand_from a d h rn u" -o m -d "Set compression method"
complete -c 7z -n "__fish_seen_subcommand_from e x" -o o -d "Set output directory"
complete -c 7z -n "__fish_seen_subcommand_from a d e rn t u x" -o p -d "Set password"
complete -c 7z -n "__fish_seen_subcommand_from a d e h l rn t u x" -o r -d "Recurse subdirectories"
complete -c 7z -n "__fish_seen_subcommand_from a" -o sa -d "Set archive name mode"
complete -c 7z -o scc -d "Set charset for console I/O"
complete -c 7z -n "__fish_seen_subcommand_from e h x" -o scrc -d "Set hash function"
complete -c 7z -n "__fish_seen_subcommand_from a u" -o scs -d "Set charset for list files"
complete -c 7z -n "__fish_seen_subcommand_from a" -o sdel -d "Delete files after compression"
complete -c 7z -n "__fish_seen_subcommand_from a u" -o seml -d "Send archive by email"
complete -c 7z -n "__fish_seen_subcommand_from a d u" -o sfx -d "Create SFX archive"
complete -c 7z -n "__fish_seen_subcommand_from a e h u x" -o si -d "Read data from stdin"
complete -c 7z -o slp -d "Set large pages mode"
complete -c 7z -n "__fish_seen_subcommand_from l" -o slt -d "Show technical information"
complete -c 7z -n "__fish_seen_subcommand_from a e u x" -o sni -d "Store NT security information"
complete -c 7z -n "__fish_seen_subcommand_from a d e h l t u x" -o sns -d "Store NTFS alternate streams"
complete -c 7z -o snh -d "Store hard links"
complete -c 7z -o snl -d "Store symbolic links"
complete -c 7z -n "__fish_seen_subcommand_from a e u x" -o so -d "Write data to stdout"
complete -c 7z -o spd -d "Disable wildcard matching"
complete -c 7z -o spe -d "Eliminate duplication of root folder"
complete -c 7z -n "__fish_seen_subcommand_from a d e u x" -o spf -d "Use fully qualified file paths"
complete -c 7z -n "__fish_seen_subcommand_from a d e l t u x" -o ssc -d "Set sensitive case mode"
complete -c 7z -n "__fish_seen_subcommand_from a h u" -o ssw -d "Compress shared files"
complete -c 7z -n "__fish_seen_subcommand_from a d rn u" -o stl -d "Set archive timestamp"
complete -c 7z -o stm -d "Set CPU thread affinity mask"
complete -c 7z -n "__fish_seen_subcommand_from a d e l t u x" -o stx -d "Exclude archive type"
complete -c 7z -n "__fish_seen_subcommand_from a d e l t u x" -o t -d "Type of archive"
complete -c 7z -n "__fish_seen_subcommand_from a d rn u" -o u -d "Update options"
complete -c 7z -n "__fish_seen_subcommand_from a" -o v -d "Create volumes"
complete -c 7z -n "__fish_seen_subcommand_from a d rn u" -o w -d "Assign work directory"
complete -c 7z -n "__fish_seen_subcommand_from a d e h l rn t u x" -o x -d "Exclude filenames"
complete -c 7z -n "__fish_seen_subcommand_from e x" -o y -d "Assume yes on all queries"

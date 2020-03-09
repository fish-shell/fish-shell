# Completions for patool

# Global options
complete -f -c patool -s h -l help -d "Show help message"
complete -f -c patool -n __fish_no_arguments -s v -l verbose -d "Be verbose"
complete -f -c patool -n __fish_no_arguments -l non-interactive -d "Run in batch mode"

# Commands
complete -f -c patool -n __fish_use_subcommand -a extract -d "Extract archive(s)"
complete -f -c patool -n __fish_use_subcommand -a list -d "List files in archive(s)"
complete -f -c patool -n __fish_use_subcommand -a create -d "Create archive"
complete -f -c patool -n __fish_use_subcommand -a test -d "Test archive(s)"
complete -f -c patool -n __fish_use_subcommand -a repack -d "Repackage archive to different format"
complete -f -c patool -n __fish_use_subcommand -a recompress -d "Recompress archive to smaller"
complete -f -c patool -n __fish_use_subcommand -a diff -d "Differences between two archives"
complete -f -c patool -n __fish_use_subcommand -a search -d "Search contents of archive"
complete -f -c patool -n __fish_use_subcommand -a formats -d "Show supported formats"

# extract option
complete -c patool -n "__fish_seen_subcommand_from extract" -l outdir -d "Specify output directory"

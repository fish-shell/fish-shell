function __forfiles_generate_args --description 'Function to generate args'
  echo -e '/P\tSpecify the path from which to start the search
/M\tSearch files according to the specified search mask
/S\tInstruct the forfiles command to search in subdirectories recursively
/C\tRun the specified command on each file
/D\tSelect files with a last modified date within the specified time frame
/?\tShow help'
end

complete --command forfiles --no-files --arguments '(__forfiles_generate_args)'

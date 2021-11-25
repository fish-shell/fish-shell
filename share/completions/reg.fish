function __reg_generate_args --description 'Function to generate args'
  if not __fish_seen_subcommand_from add compare copy delete export import load query restore save unload
    echo -e 'add\tAdd a new subkey or entry to the registry
compare\tCompare specified registry subkeys or entries
copy\tCopy a registry entry to a specified location on the local or remote computer
delete\tDelete a subkey or entries from the registry
export\tCopy the specified subkeys, entries, and values of the local computer into a file
import\tCopy the contents of a file that contains registry data into the registry of the local computer
load\tWrite saved subkeys and entries into a different subkey in the registry
query\tReturn a list of the next tier of subkeys and entries
restore\tWrite saved subkeys and entries back to the registry
save\tSave a copy of specified subkeys, entries, and values of the registry in a specified file
unload\tRemove a section of the registry that was loaded using the reg load operation'
    return
  end

  echo -e '/?\tShow help'

  if __fish_seen_subcommand_from add
    if not __fish_seen_argument --windows '/v' --windows '/ve'
      echo -e '/v\tSpecify the name of the add registry entry
/ve\tSpecify that the added registry entry has a null value'
    end

    echo -e '/t\tSpecify the type for the registry entry
/s\tSpecify the character to be used to separate multiple instances of data
/d\tSpecify the data for the new registry entry
/f\tAdd the registry entry without prompting for confirmation'
  else if __fish_seen_subcommand_from compare
    if not __fish_seen_argument --windows '/v' --windows '/ve'
      echo -e '/v\tSpecify the value name to compare under the subkey
/ve\tSpecify that only entries that have a value name of null should be compared'
    end

    if not __fish_seen_argument --windows '/oa' --windows '/od' --windows '/os' --windows '/on'
      echo -e '/oa\tSpecify that all differences and matches are displayed
/od\tSpecify that only differences are displayed
/os\tSpecify that only matches are displayed
/on\tSpecify that nothing is displayed'
    end

    echo -e '/s\tSpecify the type for the registry entry'
  else if __fish_seen_subcommand_from copy
    echo -e '/s\tCopy all subkeys and entries under the specified subkey
/f\tCopy the subkey without prompting for confirmation'
  else if __fish_seen_subcommand_from delete
    if not __fish_seen_argument --windows '/v' --windows '/ve' --windows '/va'
      echo -e '/v\tDelete a specific entry under the subkey
/ve\tSpecify that only entries that have no value will be deleted
/va\tDelete all entries under the specified subkey'
    end

    echo -e '/f\tDelete the existing registry subkey or entry without asking for confirmation'
  else if __fish_seen_subcommand_from export
    echo -e '/y\tOverwrite any existing file with the name filename without prompting for confirmation'
  else if __fish_seen_subcommand_from query
    if not __fish_seen_argument --windows '/v' --windows '/ve'
      echo -e '/v\tSpecify the registry value name that is to be queried
/ve\tRun a query for value names that are empty'
    end

    if not __fish_seen_argument --windows '/k' --windows '/d'
      echo -e '/k\tSpecify to search in key names only
/d\tSpecify to search in data only'
    end

    echo -e '/se\tSpecify the single value separator to search for in the value name type REG_MULTI_SZ
/f\tSpecify the data or pattern to search for
/c\tSpecify that the query is case sensitive
/e\tSpecify to return only exact matches
/t\tSpecify registry types to search
/z\tSpecify to include the numeric equivalent for the registry type in search results'
  else if __fish_seen_subcommand_from save
    echo -e '/y\tOverwrite an existing file with the name filename without prompting for confirmation'
  end
end

complete --command reg --no-files --arguments '(__reg_generate_args)'
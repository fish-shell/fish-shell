# NAME
#   spec.functions - return functions in the global scope that match ^key
#
# SYNOPSIS
#   spec.functions [OPTIONS] <key>
#
# OPTIONS
#   -b --backup <key> <prefix>
#     Create new backup functions for all functions by <key> with a <prefix>
#
#   -r --restore <prefix>
#     Create new functions without <prefix>.
#
#   -q --quiet
#     Quiet mode. No output. Return 1 if no functions are found.
#
#   -e --erase <key>
#     Erase the matched functions.
#
# EXAMPLES
#   1) spec.functions "describe_"
#   2) spec.functions -q "before_"
#
#   3) spec.functions -be "describe_" "it_" -- bak_
#      Backup all describe_* and it_* functions to bak_describe_* and
#      bak_it_* and erases the original describe_* and it_*
#
#   4) spec.functions -r bak_
#      Find all bak_* functions and copies them without the prefix bak_.
#
# AUTHORS
#   Jorge Bucaran <@bucaran>
#/
function spec.functions
  # Parse custom options
  set -l backup
  set -l prefix
  set -l restore
  set -l quiet
  set -l erase
  set -l keys
  for index in (seq (count $argv))
    switch $argv[$index]
      case -q --quiet
        set quiet -q
      # N-switch-fallthrough pattern in fish.
      case -b\* -r\* -e\*
        switch $argv[$index]
          case -b -be -eb --backup
            set backup -b
            if set -l separator (contains -i -- "--" $argv[$index..-1])
              set prefix $argv[(math $separator + 1)]
            else
              set prefix $argv[(math $index + 2)]
            end
        end
        switch $argv[$index]
          case -e -be -eb --erase -re -er
            set erase -e
        end
        switch $argv[$index]
          case -r -re -er --restore
            # Using restore takes only one argument, <prefix>
            set restore -r
            set prefix $argv[(math $index + 1)]
        end
      case \*
        set keys $keys $argv[$index]
    end
  end

  # Skip empty strings to avoid fetching all global functions.
  if [ -n "$keys" ]
    if [ -n "$restore" -a -n "$prefix" ]
      set keys $prefix
    end

    set -l list
    for key in $keys
      set list $list (functions -n | grep \^"$key")
    end

    if [ -n "$list" ]
      if [ -n "$erase" -o -n "$prefix" ]

        for item in $list
          if [ -n "$backup" -a "$prefix" ]
            functions -c $item $prefix$item
          end
          if [ -n "$restore" -a "$prefix" ]
            # Cut prefix from function and create a new copy.
            functions -c $item (echo $item | \
                cut -c (echo $prefix | awk '{ print length + 1 }')-)
          end
          if [ -n "$erase" ]
            functions -e $item
          end
        end

      else if [ -z "$quiet" ]
        printf "%s\n" $list
      end
      return 0
    end
  end
end

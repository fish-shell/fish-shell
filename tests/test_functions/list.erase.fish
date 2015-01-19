# NAME
#   list.erase - erase any items from one or more lists
#
# SYNOPSIS
#   <item> [<item>...] [--from] <list>
#   <item> [<item>...] --from <list1> [<list2>...]
#
# DESCRIPTION
#   Erase any number of items from any number of lists. If more than one
#   list is specified it must be separated from the items with --from.
#
# NOTES
#   While items are basically any valid sequence of symbols, lists refer
#   to any global variable or local variable in the scope of the calling
#   function by name.
#
# AUTHORS
#   Jorge Bucaran <@bucaran>
#/
function -S list.erase
  # Assume no items were erased.
  set -l result 1
  # At least one list should be at the last index.
  set -l items $argv[1..-2]
  set -l lists $argv[-1]
  if set -l index (contains -i -- --from $argv)
    # <items to erase> --from <lists>
    set items $argv[1..(math $index-1)]
    set lists $argv[(math $index+1)..-1]
  end
  for item in $items
    for list in $lists
      if set -l index (contains -i -- $item $$list)
        set -e $list[1][$index]
        # Function succeeds if at least an item is erased.
        set result 0
      end
    end
  end
  return $result
end

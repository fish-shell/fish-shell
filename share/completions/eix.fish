# Completions for the eix tool
# http://dev.croup.de/proj/eix and http://sourceforge.net/projects/eix/

# Author: Tassilo Horn <tassilo@member.fsf.org>

##########
# EIX
#####
# Global options
###
# exclusive options
complete -c eix -s h -l help -d "Show a help screen and exit"
complete -c eix -s V -l version -d "Show version and exit"
complete -c eix -l dump -d "Dump variables to stdout"
complete -c eix -l dump-defaults -d "Dump default values of variables to stdout"
###
# special options
complete -c eix -s t -l test-non-matching -d "Print non-matching entries first"
complete -c eix -s Q -l quick -d "Don't read unguessable slots of installed packages (toggle)"
complete -c eix -l care -d "Always read slots of installed packages"
###
# ootput options
complete -c eix -s q -l quiet -d "(no) output (toggle)"
complete -c eix -s n -l nocolor -d "Don't use ANSI color codes"
complete -c eix -s F -l force-color -d "Force colorful output"
complete -c eix -s c -l compact -d "Compact search results (toggle)"
complete -c eix -s v -l verbose -d "Verbose search results (toggle)"
complete -c eix -s x -l versionsort -d "Sort output by slots/versions (toggle)"
complete -c eix -s l -l versionlines -d "Print available versions line-by-line (toggle)"
complete -c eix -l format -d "Format string for normal output"
complete -c eix -l format-compact -d "Format string for compact output"
complete -c eix -l format-verbose -d "Format string for verbose output"
#####
# Local options
###
# Miscellaneous
complete -c eix -s I -l installed -d "Next expression only matches installed packages"
complete -c eix -s i -l multi-installed -d "Match packages installed in several versions"
complete -c eix -s d -l dup-packages -d "Match duplicated packages"
complete -c eix -s D -l dup-versions -d "Match packages with duplicated versions"
complete -c eix -s 1 -l slotted -d "Match packages with a nontrivial slot"
complete -c eix -s 2 -l slots -d "Match packages with two different slots"
complete -c eix -s u -l update -d "Match packages without best slotted version"
complete -c eix -s o -l overlay -d "Match packages from overlays"
complete -c eix -s T -l test-obsolete -d "Match packages with obsolete entries"
complete -c eix -s ! -l not -d "Invert the expression (toggle)"
complete -c eix -s '|' -l pipe -d "Use input from pipe of emerge -pv"
###
# Search Fields
complete -c eix -s S -l description -d "Search the description field"
complete -c eix -s A -l category-name -d "Search the category and name fields"
complete -c eix -s C -l category -d "Search the category field"
complete -c eix -s s -l name -d "Search the name field (default)"
complete -c eix -s H -l homepage -d "Search the homepage field"
complete -c eix -s L -l license -d "Search the license field"
complete -c eix -s P -l provides -d "Search the provides field"
###
# Type of Pattern
complete -c eix -s r -l regex -d "Pattern is a regexp (default)"
complete -c eix -s e -l exact -d "Pattern is the exact string"
complete -c eix -s p -l pattern -d "Pattern is a wildcards-pattern"
complete -c eix -s f -l fuzzy -d "Use fuzzy-search with the given max. levenshtein-distance (default: 2)"
#####
##########

# SPDX-FileCopyrightText: © 2007 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#
# Command specific completions for the xgettext command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -c xgettext -s f -l files-from -d 'Get list of input files from FILE'
complete -c xgettext -s D -l directory -d 'Add DIRECTORY to list for input files search'
complete -c xgettext -s d -l default-domain -d 'Use NAME'
complete -c xgettext -s o -l output -d 'Write output to specified file'
complete -c xgettext -s p -l output-dir -d 'Output files will be placed in directory DIR'
complete -c xgettext -s L -l language -d 'Recognise the specified programming language'
complete -c xgettext -s C -l c++ -d 'Shorthand for --language=C++'
complete -c xgettext -l from-code -d 'Encoding of input files (except for Python, Tcl, Glade)'
complete -c xgettext -s j -l join-existing -d 'Join messages with existing file'
complete -c xgettext -s x -l exclude-file -d 'Entries from FILE'
complete -c xgettext -s c -l add-comments -d 'Place comment block with TAG (or those preceding keyword lines) in output file'
complete -c xgettext -s a -l extract-all -d 'Extract all strings (only some languages)'
complete -c xgettext -s k -l keyword -d 'Look for this as an additional keyword'
complete -c xgettext -l flag -d 'Additional flag for strings inside the argument number ARG of keyword WORD'
complete -c xgettext -s T -l trigraphs -d 'Understand ANSI C trigraphs for input (only languages C, C++, ObjectiveC)'
complete -c xgettext -l qt -d 'Recognize Qt format strings (only language C++)'
complete -c xgettext -l boost -d 'Recognize Boost format strings (only language C++)'
complete -c xgettext -l debug -d 'More detailed formatstring recognition result'
complete -c xgettext -s e -l no-escape -d 'Do not use C escapes in output (default)'
complete -c xgettext -s E -l escape -d 'Use C escapes in output, no extended chars'
complete -c xgettext -l force-po -d 'Write PO file even if empty'
complete -c xgettext -s i -l indent -d 'Write the'
complete -c xgettext -l no-location -d 'Do not write #: filename:line lines'
complete -c xgettext -s n -l add-location -d 'Generate #: filename:line lines (default)'
complete -c xgettext -l strict -d 'Write out strict Uniforum conforming'
complete -c xgettext -l properties-output -d 'Write out a Java'
complete -c xgettext -l stringtable-output -d 'Write out a NeXTstep/GNUstep'
complete -c xgettext -s w -l width -d 'Set output page width'
complete -c xgettext -l no-wrap -d 'Do not break long message lines into several lines'
complete -c xgettext -s s -l sort-output -d 'Generate sorted output'
complete -c xgettext -s F -l sort-by-file -d 'Sort output by file location'
complete -c xgettext -l omit-header -d 'Dont write header with msgid "" entry'
complete -c xgettext -l copyright-holder -d 'Set copyright holder in output'
complete -c xgettext -l foreign-user -d 'Omit FSF copyright in output for foreign user'
complete -c xgettext -l msgid-bugs-address -d 'Set report address for msgid bugs'
complete -c xgettext -s m -l msgstr-prefix -d 'Use STRING or "" as prefix for msgstr entries'
complete -c xgettext -s M -l msgstr-suffix -d 'Use STRING or "" as suffix for msgstr entries'
complete -c xgettext -s h -l help -d 'Display this help and exit'
complete -c xgettext -s V -l version -d 'Output version information and exit'

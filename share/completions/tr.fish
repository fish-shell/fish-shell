#
# Completions for tr
#

# Test if we are using GNU tr
if command tr --version >/dev/null ^/dev/null
  complete -c tr -x
  complete -c tr -s c -s C -l complement -d 'use the complement of SET1'
  complete -c tr -s d -l delete          -d 'delete characters in SET1, do not translate'
  complete -c tr -s s -l squeeze-repeats -d 'replace each input sequence of a repeated character that is listed in SET1 with a single occurrence of that character'
  complete -c tr -s t -l truncate-set1   -d 'first truncate SET1 to length of SET2'
  complete -c tr -l help                 -d 'display this help and exit'
  complete -c tr -l version              -d 'output version information and exit'

  complete -c tr -a '[:alnum:]'  -d 'all letters and digits'
  complete -c tr -a '[:alpha:]'  -d 'all letters'
  complete -c tr -a '[:blank:]'  -d 'all horizontal whitespace'
  complete -c tr -a '[:cntrl:]'  -d 'all control characters'
  complete -c tr -a '[:digit:]'  -d 'all digits'
  complete -c tr -a '[:graph:]'  -d 'all printable characters, not including space'
  complete -c tr -a '[:lower:]'  -d 'all lower case letters'
  complete -c tr -a '[:print:]'  -d 'all printable characters, including space'
  complete -c tr -a '[:punct:]'  -d 'all punctuation characters'
  complete -c tr -a '[:space:]'  -d 'all horizontal or vertical whitespace'
  complete -c tr -a '[:upper:]'  -d 'all upper case letters'
  complete -c tr -a '[:xdigit:]' -d 'all hexadecimal digits'
else
  # If not a GNU system, assume we have standard BSD tr features instead
  complete -c tr -x
  complete -c tr -s C -d 'Complement the set of characters in string1.'
  complete -c tr -s c -d 'Same as -C but complement the set of values in string1.'
  complete -c tr -s d -d 'Delete characters in string1 from the input.'
  complete -c tr -s s -d 'Squeeze multiple occurrences of the characters listed in the last operand (either string1 or string2) in the input into a single instance of the character.'
  complete -c tr -l u -d 'Guarantee that any output is unbuffered.'

  complete -c tr -a '[:alnum:]'     -d 'alphanumeric characters'
  complete -c tr -a '[:alpha:]'     -d 'alphabetic characters'
  complete -c tr -a '[:blank:]'     -d 'whitespace characters'
  complete -c tr -a '[:cntrl:]'     -d 'control characters'
  complete -c tr -a '[:digit:]'     -d 'numeric characters'
  complete -c tr -a '[:graph:]'     -d 'graphic characters'
  complete -c tr -a '[:ideogram:]'  -d 'ideographic characters'
  complete -c tr -a '[:lower:]'     -d 'lower-case alphabetic characters'
  complete -c tr -a '[:phonogram:]' -d 'phonographic characters'
  complete -c tr -a '[:print:]'     -d 'printable characters'
  complete -c tr -a '[:punct:]'     -d 'punctuation characters'
  complete -c tr -a '[:rune:]'      -d 'valid characters'
  complete -c tr -a '[:space:]'     -d 'space characters'
  complete -c tr -a '[:special:]'   -d 'special characters'
  complete -c tr -a '[:upper:]'     -d 'upper-case characters'
  complete -c tr -a '[:xdigit:]'    -d 'hexadecimal characters'
end

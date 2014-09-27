\section echo echo - display a line of text

\subsection echo-synopsis Synopsis
\fish{synopsis}
echo [OPTIONS] [STRING]
\endfish

\subsection echo-description Description

`echo` displays a string of text.

The following options are available:

- `-n`, Do not output a newline

- `-s`, Do not separate arguments with spaces

- `-E`, Disable interpretation of backslash escapes (default)

- `-e`, Enable interpretation of backslash escapes

\subsection echo-escapes Escape Sequences

If `-e` is used, the following sequences are recognized:

- `\` backslash

- `\a` alert (BEL)

- `\b` backspace

- `\c` produce no further output

- `\e` escape

- `\f` form feed

- `\n` new line

- `\r` carriage return

- `\t` horizontal tab

- `\v` vertical tab

- `\0NNN` byte with octal value NNN (1 to 3 digits)

- `\xHH` byte with hexadecimal value HH (1 to 2 digits)

\subsection echo-example Example

\fish
echo 'Hello World'
\endfish
Print hello world to stdout

\fish
echo -e 'Top\\nBottom'
\endfish
Print Top and Bottom on separate lines, using an escape sequence

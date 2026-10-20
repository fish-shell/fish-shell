#RUN: %fish %s

prompt_pwd -d 1 /foo/bar/baz
# CHECK: /f/b/baz

prompt_pwd /usr/share/fish/prompts
# CHECK: /u/s/f/prompts

prompt_pwd -D 2 /usr/share/fish/prompts
# CHECK: /u/s/fish/prompts

prompt_pwd -D 0 /usr/share/fish/prompts
# CHECK: /u/s/f/p

prompt_pwd -d1 -D 3 /usr/local/share/fish/prompts
# CHECK: /u/l/share/fish/prompts

# Ensure control characters in paths are stripped
prompt_pwd -d 0 /foo/(printf '\e]0;OHNO\a')bar
# CHECK: /foo/]0;OHNObar

# Ensure newlines in directory names are stripped (issue #12789)
prompt_pwd -d 0 "/foo/b
3"
# CHECK: /foo/b3

prompt_pwd -d 0 "/foo/x
y
z"
# CHECK: /foo/xyz

echo "["(prompt_pwd -d 0 "/foo/b
")"]"
# CHECK: [/foo/b]

prompt_pwd -d 0 "/foo/
b"
# CHECK: /foo/b

# Default shortening with an embedded newline
prompt_pwd "/foo/b
3"
# CHECK: /f/b3

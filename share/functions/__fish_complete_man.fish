function __fish_complete_man
    # Try to guess what section to search in. If we don't know, we
    # use [^)]*, which should match any section.
    set -l section ""
    set -l token (commandline -ct)
    set -l prev (commandline -pxc)
    set -e prev[1]
    while set -q prev[1]
        switch $prev[1]
            case '-**'

            case '*'
                set section (string escape --style=regex $prev[1])
                set section (string replace --all / \\/ $section)
        end
        set -e prev[1]
    end

    set -l exclude_fish_commands
    # Only include fish commands when section is empty or 1
    if test -z "$section" -o "$section" = 1
        set -e exclude_fish_commands
    end

    set section $section"[^)]*"
    # If we don't have a token but a section, list all pages for that section.
    # Don't do it for all sections because that would be overwhelming.
    if test -z "$token" -a "$section" != "[^)]*"
        set token "."
    end

    if test -n "$token"
        # Do the actual search
        __fish_apropos ^$token 2>/dev/null | awk '
                BEGIN { FS="[\t ]- "; OFS="\t"; }
                # BSD/Darwin
                /^[^( \t]+(, [^( \t]+)*\('$section'\)/ {
                  paren = index($1, "(");
                  sect = substr($1, paren + 1, length($1) - paren - 1);
                  aliases = substr($1, 1, paren - 1)
                  split(aliases, pages, ", ");
                  for (i in pages) {
                    print pages[i], sect ": " $2
                  }
                }
                # man-db
                /^[^( \t]+ +\('$section'\)/ {
                  split($1, t, " ");
                  sect = substr(t[2], 2, length(t[2]) - 2);
                  print t[1], sect ": " $2;
                }
                # man-db RHEL 5 with [aliases]
                /^[^( \t]+ +\[.*\] +\('$section'\)/ {
                  split($1, t, " ");
                  sect = substr(t[3], 2, length(t[3]) - 2);
                  print t[1], sect ": " $2;
                }   
                # Solaris 11
                # Does not display descriptions
                # Solaris apropos outputs embedded backspace in descriptions
                /^[0-9]+\. [^( \t]*\('$section'\) / {
                  split($1, t, " ")
                  paren = index(t[2], "(");
                  name = substr(t[2], 1, paren - 1);
                  sect = substr(t[2], paren + 1, length(t[2]) - paren - 1);
                  print name, sect
                }
                '

        # Fish commands are not given by apropos
        if not set -ql exclude_fish_commands
            set -l files $__fish_data_dir/man/man1/*.1
            string replace -r '.*/([^/]+)\.1$' '$1\t1: fish command' -- $files
        end
    else
        return 1
    end
    return 0
end

#!/usr/bin/env fish
# Build a list of all sections in the html sphinx docs, separately by page,
# so it can be added to share/functions/help.fish
# Use like
#    fish extract_help_sections.fish user_doc/html/{fish_for_bash_users.html,faq.html,interactive.html,language.html,tutorial.html}
# TODO: Currently `help` uses variable names we can't generate, so it needs to be touched up manually.
# Also this could easily be broken by changes in sphinx, ideally we'd have a way to let it print the section titles.
#

for file in $argv
    set -l varname (string replace -r '.*/(.*).html' '$1' -- $file | string escape --style=var)pages
    # Technically we can use any id in the document as an anchor, but listing them all is probably too much.
    # Sphinx stores section titles (in a slug-ized form) in the id,
    # and stores explicit section links in a `span` tag like
    # `<span id="identifiers"></span>`
    # We extract both separately.
    set -l sections (string replace -rf '.*class="headerlink" href="#([^"]*)".*' '$1' <$file)
    # Sections titled "id5" and such are internal cruft and shouldn't be offered.
    set -a sections (string replace -rf '.*span id="([^"]*)".*' '$1' <$file | string match -rv 'id\d+')

    set sections (printf '%s\n' $sections | sort -u)
    echo set -l $varname $sections
end

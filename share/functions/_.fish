#
# Alias for gettext or a fallback if gettext isn't installed.
#
# Use ggettext if available.
# This is the case on OpenIndiana, where the default gettext
# interprets `\n` itself, so
#    printf (_ 'somemessage\n')
# won't print a newline.
if command -sq ggettext
    function _ --description "Alias for the ggettext command"
        command ggettext fish $argv
    end
else if command -sq gettext
    function _ --description "Alias for the gettext command"
        command gettext fish $argv
    end
else
    function _ --description "Fallback alias for the gettext command"
        echo -n $argv
    end
end


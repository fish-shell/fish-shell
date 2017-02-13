#
# Alias for gettext or a fallback if gettext isn't installed.
#
if command -sq gettext
    function _ --description "Alias for the gettext command"
        command gettext fish $argv
    end
else
    function _ --description "Fallback alias for the gettext command"
        echo -n $argv
    end
end


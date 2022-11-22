# Completion for `konsole



function __konsole_generate_args --description 'Function to generate args'
    set --local current_token (commandline --current-token --cut-at-cursor)
    switch $current_token
        case '*'
            echo -e "--help\tShow help and exit
-v\tShow version and exit
--version\tShow version and exit
--help-qt\tShow Qt specific options and exit
--help-kde\tShow KDE specific options and exit
--help-all\tShow all options and exit
--author\tShow author information and exit
--license\tShow license information and exit
--name\tWindow class
--ls\tLogin shell
-T\tWindow name
--tn\tTerminal type
--noclose\tDo not close when command exists
--nohist\tDo not save history
--nomenubar\tHide menu bar
--notabbar\tHide tab bar
--notoolbar\tHide tab bar
--noframe\tHide frame bar
--noscrollbar\tHide scrollbar
--noxft\tDo not use atni-aliasing
--noresize\tDo not allow window to be resized
--vt_sz\tWindow size
--type\tSession type
--types\tList session types
--keytab\tKeytab name
--keytabs\tList keytabs
--profile\tProfile
--profiles\tList profiles
--schema\tSchema
--schemas\tList schemas
--script\tEnable extended DCOP Qt functions
--workdir\tWorking directory
-e\tCommand to execute"
    end
end

complete --command konsole --arguments '(__konsole_generate_args)'

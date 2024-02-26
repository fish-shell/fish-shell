# Completions for sv
# A part of the runit init system
# Author: Leonardo da Rosa Eugênio <lelgenio@disroot.org>

set -l commands \
    status up down once s u d o pause cont hup \
    alarm interrupt quit 1 2 term kill exit p c h \
    a i q 1 2 t k e start stop reload restart \
    shutdown force-stop force-reload force-restart force-shutdown \
    try-restart check

function __fish_complete_sv_list_services
    set -l svdir (path filter -d -- $SVDIR  \
        /run/runit/runsvdir/current \
        /run/runit/service \
        /etc/services \
        /services)
    set -q svdir[1]; or return
    set -l services (path basename -- $svdir[1]/*)
    set -l sv_status (sv status $services 2>/dev/null |
                      string replace -ar ';.*$' '')
    and string replace -r "^(\w+: )(.*?):" '$2\t$1' $sv_status
    or printf "%s\n" $services
end

complete -f -c sv -a "(__fish_complete_sv_list_services)" -n "__fish_seen_subcommand_from $commands"

complete -fc sv -s v -d "Report status for up, down, term, once, cont, and exit"
complete -fc sv -s w -d "Override the default timeout to report status"

set -l no_comm "not __fish_seen_subcommand_from $commands"

complete -kfc sv -n $no_comm -a check -d "Check if the service is in it's requested state"
complete -kfc sv -n $no_comm -a try-restart -d "Run term, cont, and  up, report status"
complete -kfc sv -n $no_comm -a force-shutdown -d "Run exit, report status or kill on timeout"
complete -kfc sv -n $no_comm -a force-restart -d "Run term, cont and up, report status"
complete -kfc sv -n $no_comm -a force-reload -d "Run term and  cont, report status"
complete -kfc sv -n $no_comm -a force-stop -d "Run down, report status or kill on timeout"
complete -kfc sv -n $no_comm -a shutdown -d "Run exit, report status"
complete -kfc sv -n $no_comm -a restart -d "Run term, cont, and  up, report status using ./check"
complete -kfc sv -n $no_comm -a reload -d "Run hup, report status"
complete -kfc sv -n $no_comm -a stop -d "Run down, report status"
complete -kfc sv -n $no_comm -a start -d "Run up, report status"

complete -kfc sv -n $no_comm -a e -d "Alias for exit"
complete -kfc sv -n $no_comm -a k -d "Alias for kill"
complete -kfc sv -n $no_comm -a t -d "Alias for term"
complete -kfc sv -n $no_comm -a 2 -d "Alias for 2"
complete -kfc sv -n $no_comm -a 1 -d "Alias for 1"
complete -kfc sv -n $no_comm -a q -d "Alias for quit"
complete -kfc sv -n $no_comm -a i -d "Alias for interrupt"
complete -kfc sv -n $no_comm -a a -d "Alias for alarm"
complete -kfc sv -n $no_comm -a h -d "Alias for hup"
complete -kfc sv -n $no_comm -a c -d "Alias for cont"
complete -kfc sv -n $no_comm -a p -d "Alias for pause"
complete -kfc sv -n $no_comm -a exit -d "Send TERM, and CONT, report status"
complete -kfc sv -n $no_comm -a kill -d "Send SIGKILL"
complete -kfc sv -n $no_comm -a term -d "Send SIGTERM"
complete -kfc sv -n $no_comm -a 2 -d "Send SIGUSR2"
complete -kfc sv -n $no_comm -a 1 -d "Send SIGUSR1"
complete -kfc sv -n $no_comm -a quit -d "Send SIGQUIT"
complete -kfc sv -n $no_comm -a interrupt -d "Send SIGINT"
complete -kfc sv -n $no_comm -a alarm -d "Send SIGALRM"
complete -kfc sv -n $no_comm -a hup -d "Send SIGHUP"
complete -kfc sv -n $no_comm -a cont -d "Send SIGCONT"
complete -kfc sv -n $no_comm -a pause -d "Send SIGSTOP"

complete -kfc sv -n $no_comm -a o -d "Alias for once"
complete -kfc sv -n $no_comm -a d -d "Alias for down"
complete -kfc sv -n $no_comm -a u -d "Alias for up"
complete -kfc sv -n $no_comm -a s -d "Alias for status"
complete -kfc sv -n $no_comm -a once -d "Start service, but don't restart it"
complete -kfc sv -n $no_comm -a down -d "Send it the TERM signal"
complete -kfc sv -n $no_comm -a up -d "Start a service"
complete -kfc sv -n $no_comm -a status -d "Report the current status of the service"

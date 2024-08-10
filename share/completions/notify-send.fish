set -l c complete -c notify-send

$c -f # Disable file completion

# notify-send [OPTION…] <SUMMARY> [BODY] - create a notification

set -l urgency_levels

$c -s u -l urgency -d "urgency level" -x -a 'low normal critical'
$c -s t -l expire-time -x -d "timeout in milliseconds at which to expire the notification"
$c -s a -l app-name -x -d "app name for the icon"
$c -s i -l icon -x -d "icon filename or stock icon to display"
$c -s i -l icon -x -a "(__fish_complete_freedesktop_icons)"
$c -s c -l category -x -d "notification category"
$c -s e -l transient -d "create a transient notification"
$c -s h -l hint -x -d "extra data to pass, format: `TYPE:NAME:VALUE`, valid types: boolean, int, double, string, byte and variant"
$c -s p -l print-id -d "print the notification ID"
$c -s r -l replace-id -x -d "ID of the notification to replace"
$c -s w -l wait -d "wait for the notification to be closed before exciting"
$c -s A -l action -x -d "clickable actions to display to the user. implies --wait, can be set multiple times, example: --action=save='Save Screenshot?'"

$c -s '?' -l help -d "print help information"
$c -s v -l version -d "print program version"

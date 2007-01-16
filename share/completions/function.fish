complete -c function -s d -l description --description "Set function description" -x
complete -c function -xa "(functions -n)" --description "Function"
complete -c function -xa "(builtin -n)" --description "Builtin"
complete -c function -s j -l on-job-exit --description "Make the function a job exit event handler" -x
complete -c function -s p -l on-process-exit --description "Make the function a process exit event handler" -x
complete -c function -s s -l on-signal --description "Make the function a signal event handler" -x
complete -c function -s v -l on-variable --description "Make the function a variable update event handler" -x
complete -c function -s b -l key-binding --description "Allow dash (-) in function name"

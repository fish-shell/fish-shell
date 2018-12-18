exec - execute command in current process
==========================================

Synopsis
--------

exec COMMAND [OPTIONS...]


\subsection exec-description Description

`exec` replaces the currently running shell with a new command. On successful completion, `exec` never returns. `exec` cannot be used inside a pipeline.


\subsection exec-example Example

`exec emacs` starts up the emacs text editor, and exits `fish`. When emacs exits, the session will terminate.

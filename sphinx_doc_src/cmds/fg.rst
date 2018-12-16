\section fg fg - bring job to foreground

\subsection fg-synopsis Synopsis
\fish{synopsis}
fg [PID]
\endfish

\subsection fg-description Description

`fg` brings the specified <a href="index.html#syntax-job-control">job</a> to the foreground, resuming it if it is stopped. While a foreground job is executed, fish is suspended. If no job is specified, the last job to be used is put in the foreground. If PID is specified, the job with the specified group ID is put in the foreground.


\subsection fg-example Example

`fg` will put the last job in the foreground.

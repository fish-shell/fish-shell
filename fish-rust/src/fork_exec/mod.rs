// A mdoule concerned with the exec side of fork/exec.
// This concerns posix_spawn support, and async-signal
// safe code which happens in between fork and exec.

mod flog_safe;

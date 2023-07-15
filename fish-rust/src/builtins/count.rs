use super::prelude::*;

// How many bytes we read() at once.
// Since this is just for counting, it can be massive.
const COUNT_CHUNK_SIZE: usize = 512 * 256;

/// Implementation of the builtin count command, used to count the number of arguments sent to it.
pub fn count(_parser: &Parser, streams: &mut IoStreams<'_>, argv: &mut [WString]) -> Option<c_int> {
    // Always add the size of argv (minus 0, which is "count").
    // That means if you call `something | count a b c`, you'll get the count of something _plus 3_.
    let mut numargs = argv.len() - 1;

    // (silly variable for Arguments to do nothing with)
    let mut zero = 0;

    // Count the newlines coming in via stdin like `wc -l`.
    // This means excluding lines that don't end in a newline!
    numargs += Arguments::new(&[] as _, &mut zero, streams, COUNT_CHUNK_SIZE)
        // second is "want_newline" - whether the line ended in a newline
        .filter(|x| x.1)
        .count();

    streams.out.appendln_owned(numargs.to_wstring());

    if numargs == 0 {
        return STATUS_CMD_ERROR;
    }
    STATUS_CMD_OK
}

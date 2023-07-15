use super::*;
use crate::wutil::fish_wcstol;

#[derive(Default)]
pub struct Repeat {
    count: usize,
    max: usize,
    quiet: bool,
    no_newline: bool,
}

impl StringSubCommand<'_> for Repeat {
    fn long_options(&self) -> &'static [woption<'static>] {
        const opts: &'static [woption<'static>] = &[
            wopt(L!("count"), required_argument, 'n'),
            wopt(L!("max"), required_argument, 'm'),
            wopt(L!("quiet"), no_argument, 'q'),
            wopt(L!("no-newline"), no_argument, 'N'),
        ];
        opts
    }
    fn short_options(&self) -> &'static wstr {
        L!(":n:m:qN")
    }

    fn parse_opt(&mut self, w: &mut wgetopter_t<'_, '_>, c: char) -> Result<(), StringError> {
        match c {
            'n' => {
                self.count = fish_wcstol(w.woptarg().unwrap())?.try_into().map_err(|_| {
                    invalid_args!("%ls: Invalid count value '%ls'\n", w.cmd(), w.woptarg())
                })?
            }
            'm' => {
                self.max = fish_wcstol(w.woptarg().unwrap())?.try_into().map_err(|_| {
                    invalid_args!("%ls: Invalid max value '%ls'\n", w.cmd(), w.woptarg())
                })?
            }
            'q' => self.quiet = true,
            'N' => self.no_newline = true,
            _ => return Err(StringError::UnknownOption),
        }
        return Ok(());
    }

    fn handle(
        &mut self,
        _parser: &Parser,
        streams: &mut IoStreams<'_>,
        optind: &mut usize,
        args: &[WString],
    ) -> Option<libc::c_int> {
        if self.max == 0 && self.count == 0 {
            // XXX: This used to be allowed, but returned 1.
            // Keep it that way for now instead of adding an error.
            // streams.err.append(L"Count or max must be greater than zero");
            return STATUS_CMD_ERROR;
        }

        let mut all_empty = true;
        let mut first = true;
        let mut print_trailing_newline = true;

        for (w, want_newline) in arguments(args, optind, streams) {
            print_trailing_newline = want_newline;
            if w.is_empty() {
                continue;
            }

            all_empty = false;

            if self.quiet {
                // Early out if we can - see #7495.
                return STATUS_CMD_OK;
            }

            if !first {
                streams.out.append1('\n');
            }
            first = false;

            // The maximum size of the string is either the "max" characters,
            // or it's the "count" repetitions, whichever ends up lower.
            let max = if self.max == 0
                || (self.count > 0 && w.len().wrapping_mul(self.count) < self.max)
            {
                // TODO: we should disallow overflowing unless max <= w.len().checked_mul(self.count).unwrap_or(usize::MAX)
                w.len().wrapping_mul(self.count)
            } else {
                self.max
            };

            // Reserve a string to avoid writing constantly.
            // The 1500 here is a total gluteal extraction, but 500 seems to perform slightly worse.
            let chunk_size = 1500;
            // The + word length is so we don't have to hit the chunk size exactly,
            // which would require us to restart in the middle of the string.
            // E.g. imagine repeating "12345678". The first chunk is hit after a last "1234",
            // so we would then have to restart by appending "5678", which requires a substring.
            // So let's not bother.
            //
            // Unless of course we don't even print the entire word, in which case we just need max.
            let mut chunk = WString::with_capacity(max.min(chunk_size + w.len()));

            let mut i = max;
            while i > 0 {
                if i >= w.len() {
                    chunk.push_utfstr(&w);
                } else {
                    chunk.push_utfstr(w.slice_to(i));
                    break;
                }

                i -= w.len();

                if chunk.len() >= chunk_size {
                    // We hit the chunk size, write it repeatedly until we can't anymore.
                    streams.out.append(&chunk);
                    while i >= chunk.len() {
                        streams.out.append(&chunk);
                        // We can easily be asked to write *a lot* of data,
                        // so we need to check every so often if the pipe has been closed.
                        // If we didn't, running `string repeat -n LARGENUMBER foo | pv`
                        // and pressing ctrl-c seems to hang.
                        if streams.out.flush_and_check_error() != STATUS_CMD_OK.unwrap() {
                            return STATUS_CMD_ERROR;
                        }
                        i -= chunk.len();
                    }
                    chunk.clear();
                }
            }

            // Flush the remainder.
            if !chunk.is_empty() {
                streams.out.append(&chunk);
            }
        }

        // Historical behavior is to never append a newline if all strings were empty.
        if !self.quiet && !self.no_newline && !all_empty && print_trailing_newline {
            streams.out.append1('\n');
        }

        if all_empty {
            STATUS_CMD_ERROR
        } else {
            STATUS_CMD_OK
        }
    }
}

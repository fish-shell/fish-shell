use super::*;

#[derive(Default)]
pub struct Repeat {
    count: Option<usize>,
    max: Option<usize>,
    quiet: bool,
    no_newline: bool,
}

impl StringSubCommand<'_> for Repeat {
    const LONG_OPTIONS: &'static [WOption<'static>] = &[
        wopt(L!("count"), RequiredArgument, 'n'),
        wopt(L!("max"), RequiredArgument, 'm'),
        wopt(L!("quiet"), NoArgument, 'q'),
        wopt(L!("no-newline"), NoArgument, 'N'),
    ];
    const SHORT_OPTIONS: &'static wstr = L!("n:m:qN");

    fn parse_opt(&mut self, name: &wstr, c: char, arg: Option<&wstr>) -> Result<(), StringError> {
        match c {
            'n' => {
                self.count = Some(
                    fish_wcstol(arg.unwrap())?
                        .try_into()
                        .map_err(|_| invalid_args!("%s: Invalid count value '%s'", name, arg))?,
                );
            }
            'm' => {
                self.max = Some(
                    fish_wcstol(arg.unwrap())?
                        .try_into()
                        .map_err(|_| invalid_args!(BUILTIN_ERR_INVALID_MAX_VALUE, name, arg))?,
                );
            }
            'q' => self.quiet = true,
            'N' => self.no_newline = true,
            _ => return Err(StringError::UnknownOption),
        }
        Ok(())
    }

    fn take_args(
        &mut self,
        optind: &mut usize,
        args: &[&'_ wstr],
        streams: &mut IoStreams,
    ) -> Result<(), ErrorCode> {
        if self.count.is_some() || self.max.is_some() {
            return Ok(());
        }

        let name = args[0];

        let Some(arg) = args.get(*optind) else {
            string_error!(streams, BUILTIN_ERR_ARG_COUNT0, name);
            return Err(STATUS_INVALID_ARGS);
        };
        *optind += 1;

        let Ok(Ok(count)) = fish_wcstol(arg).map(|count| count.try_into()) else {
            string_error!(streams, "%s: Invalid count value '%s'", name, arg);
            return Err(STATUS_INVALID_ARGS);
        };

        self.count = Some(count);
        Ok(())
    }

    fn handle(
        &mut self,
        _parser: &Parser,
        streams: &mut IoStreams,
        optind: &mut usize,
        args: &[&wstr],
    ) -> Result<(), ErrorCode> {
        let max = self.max.unwrap_or_default();
        let count = self.count.unwrap_or_default();

        if max == 0 && count == 0 {
            // XXX: This used to be allowed, but returned 1.
            // Keep it that way for now instead of adding an error.
            // streams.err.append(L"Count or max must be greater than zero");
            return Err(STATUS_CMD_ERROR);
        }

        let mut all_empty = true;
        let mut first = true;
        let mut print_trailing_newline = true;

        for InputValue { arg, want_newline } in arguments(args, optind, streams) {
            print_trailing_newline = want_newline;
            if arg.is_empty() {
                continue;
            }

            all_empty = false;

            if self.quiet {
                // Early out if we can - see #7495.
                return Ok(());
            }

            if !first {
                streams.out.append('\n');
            }
            first = false;

            // The maximum size of the string is either the "max" characters,
            // or it's the "count" repetitions, whichever ends up lower.
            let max_repeat_len = arg.len().wrapping_mul(count);
            let max = if max == 0 || (count > 0 && max_repeat_len < max) {
                // TODO: we should disallow overflowing unless max <= w.len().checked_mul(self.count).unwrap_or(usize::MAX)
                max_repeat_len
            } else {
                max
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
            let mut chunk = WString::with_capacity(max.min(chunk_size + arg.len()));

            let mut i = max;
            while i > 0 {
                if i >= arg.len() {
                    chunk.push_utfstr(&arg);
                } else {
                    chunk.push_utfstr(arg.slice_to(i));
                    break;
                }

                i -= arg.len();

                if chunk.len() >= chunk_size {
                    // We hit the chunk size, write it repeatedly until we can't anymore.
                    streams.out.append(&chunk);
                    while i >= chunk.len() {
                        streams.out.append(&chunk);
                        // We can easily be asked to write *a lot* of data,
                        // so we need to check every so often if the pipe has been closed.
                        // If we didn't, running `string repeat -n LARGENUMBER foo | pv`
                        // and pressing ctrl-c seems to hang.
                        if streams.out.flush_and_check_error() != STATUS_CMD_OK {
                            return Err(STATUS_CMD_ERROR);
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
            streams.out.append('\n');
        }

        if all_empty {
            Err(STATUS_CMD_ERROR)
        } else {
            Ok(())
        }
    }
}

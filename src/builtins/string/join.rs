use super::*;

pub struct Join<'args> {
    quiet: bool,
    no_empty: bool,
    pub is_join0: bool,
    sep: &'args wstr,
}

impl Default for Join<'_> {
    fn default() -> Self {
        Self {
            quiet: false,
            no_empty: false,
            is_join0: false,
            sep: L!("\0"),
        }
    }
}

impl<'args> StringSubCommand<'args> for Join<'args> {
    const LONG_OPTIONS: &'static [WOption<'static>] = &[
        wopt(L!("quiet"), NoArgument, 'q'),
        wopt(L!("no-empty"), NoArgument, 'n'),
    ];
    const SHORT_OPTIONS: &'static wstr = L!("qn");

    fn parse_opt(&mut self, _n: &wstr, c: char, _arg: Option<&wstr>) -> Result<(), StringError> {
        match c {
            'q' => self.quiet = true,
            'n' => self.no_empty = true,
            _ => return Err(StringError::UnknownOption),
        }
        Ok(())
    }

    fn take_args(
        &mut self,
        optind: &mut usize,
        args: &[&'args wstr],
        streams: &mut IoStreams,
    ) -> Result<(), ErrorCode> {
        if self.is_join0 {
            return Ok(());
        }

        let Some(arg) = args.get(*optind).copied() else {
            string_error!(streams, BUILTIN_ERR_ARG_COUNT0, args[0]);
            return Err(STATUS_INVALID_ARGS);
        };
        *optind += 1;
        self.sep = arg;

        Ok(())
    }

    fn handle(
        &mut self,
        _parser: &Parser,
        streams: &mut IoStreams,
        optind: &mut usize,
        args: &[&wstr],
    ) -> Result<(), ErrorCode> {
        let sep = self.sep;
        let mut nargs = 0usize;
        let mut print_trailing_newline = true;
        for InputValue { arg, want_newline } in arguments(args, optind, streams) {
            if !self.quiet {
                if self.no_empty && arg.is_empty() {
                    continue;
                }

                if nargs > 0 {
                    streams.out.append(sep);
                }

                streams.out.append(&arg);
            } else if nargs > 1 {
                return Ok(());
            }
            nargs += 1;
            print_trailing_newline = want_newline;
        }

        if nargs > 0 && !self.quiet {
            if self.is_join0 {
                streams.out.append('\0');
            } else if print_trailing_newline {
                streams.out.append('\n');
            }
        }

        if nargs > 1 {
            Ok(())
        } else {
            Err(STATUS_CMD_ERROR)
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::builtins::shared::{STATUS_CMD_ERROR, STATUS_CMD_OK, STATUS_INVALID_ARGS};
    use crate::tests::prelude::*;
    use crate::validate;

    #[test]
    #[serial]
    #[rustfmt::skip]
    fn plain() {
        let _cleanup = test_init();
        validate!(["string", "join"], STATUS_INVALID_ARGS, "");
        validate!(["string", "join", ""], STATUS_CMD_ERROR, "");
        validate!(["string", "join", "", "", "", ""], STATUS_CMD_OK, "\n");
        validate!(["string", "join", "", "a", "b", "c"], STATUS_CMD_OK, "abc\n");
        validate!(["string", "join", ".", "fishshell", "com"], STATUS_CMD_OK, "fishshell.com\n");
        validate!(["string", "join", "/", "usr"], STATUS_CMD_ERROR, "usr\n");
        validate!(["string", "join", "/", "usr", "local", "bin"], STATUS_CMD_OK, "usr/local/bin\n");
        validate!(["string", "join", "...", "3", "2", "1"], STATUS_CMD_OK, "3...2...1\n");
        validate!(["string", "join", "-q"], STATUS_INVALID_ARGS, "");
        validate!(["string", "join", "-q", "."], STATUS_CMD_ERROR, "");
        validate!(["string", "join", "-q", ".", "."], STATUS_CMD_ERROR, "");
    }
}

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
    const LONG_OPTIONS: &'static [woption<'static>] = &[
        wopt(L!("quiet"), no_argument, 'q'),
        wopt(L!("no-empty"), no_argument, 'n'),
    ];
    const SHORT_OPTIONS: &'static wstr = L!(":qn");

    fn parse_opt(&mut self, _n: &wstr, c: char, _arg: Option<&wstr>) -> Result<(), StringError> {
        match c {
            'q' => self.quiet = true,
            'n' => self.no_empty = true,
            _ => return Err(StringError::UnknownOption),
        }
        return Ok(());
    }

    fn take_args(
        &mut self,
        optind: &mut usize,
        args: &[&'args wstr],
        streams: &mut io_streams_t,
    ) -> Option<libc::c_int> {
        if self.is_join0 {
            return STATUS_CMD_OK;
        }

        let Some(arg) = args.get(*optind).copied() else {
            string_error!(streams, BUILTIN_ERR_ARG_COUNT0, args[0]);
            return STATUS_INVALID_ARGS;
        };
        *optind += 1;
        self.sep = arg;

        STATUS_CMD_OK
    }

    fn handle(
        &mut self,
        _parser: &mut parser_t,
        streams: &mut io_streams_t,
        optind: &mut usize,
        args: &[&wstr],
    ) -> Option<libc::c_int> {
        let sep = &self.sep;
        let mut nargs = 0usize;
        let mut print_trailing_newline = true;
        for (arg, want_newline) in arguments(args, optind, streams) {
            if !self.quiet {
                if self.no_empty && arg.is_empty() {
                    continue;
                }

                if nargs > 0 {
                    streams.out.append(sep);
                }

                streams.out.append(arg);
            } else if nargs > 1 {
                return STATUS_CMD_OK;
            }
            nargs += 1;
            print_trailing_newline = want_newline;
        }

        if nargs > 0 && !self.quiet {
            if self.is_join0 {
                streams.out.append1('\0');
            } else if print_trailing_newline {
                streams.out.append1('\n');
            }
        }

        if nargs > 1 {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        }
    }
}

use super::*;

pub struct Trim<'args> {
    chars_to_trim: &'args wstr,
    left: bool,
    right: bool,
    quiet: bool,
}

impl Default for Trim<'_> {
    fn default() -> Self {
        Self {
            // from " \f\n\r\t\v"
            chars_to_trim: L!(" \x0C\n\r\x09\x0B"),
            left: false,
            right: false,
            quiet: false,
        }
    }
}

impl<'args> StringSubCommand<'args> for Trim<'args> {
    const LONG_OPTIONS: &'static [woption<'static>] = &[
        wopt(L!("chars"), required_argument, 'c'),
        wopt(L!("left"), no_argument, 'l'),
        wopt(L!("right"), no_argument, 'r'),
        wopt(L!("quiet"), no_argument, 'q'),
    ];
    const SHORT_OPTIONS: &'static wstr = L!(":c:lrq");

    fn parse_opt(
        &mut self,
        _n: &wstr,
        c: char,
        arg: Option<&'args wstr>,
    ) -> Result<(), StringError> {
        match c {
            'c' => self.chars_to_trim = arg.unwrap(),
            'l' => self.left = true,
            'r' => self.right = true,
            'q' => self.quiet = true,
            _ => return Err(StringError::UnknownOption),
        }
        return Ok(());
    }

    fn handle(
        &mut self,
        _parser: &mut parser_t,
        streams: &mut io_streams_t,
        optind: &mut usize,
        args: &[&wstr],
    ) -> Option<libc::c_int> {
        // If neither left or right is specified, we do both.
        if !self.left && !self.right {
            self.left = true;
            self.right = true;
        }

        let mut ntrim = 0;

        let to_trim_end = |str: &wstr| -> usize {
            str.chars()
                .rev()
                .take_while(|&c| self.chars_to_trim.contains(c))
                .count()
        };

        let to_trim_start = |str: &wstr| -> usize {
            str.chars()
                .take_while(|&c| self.chars_to_trim.contains(c))
                .count()
        };

        for (arg, want_newline) in arguments(args, optind, streams) {
            let trim_start = self.left.then(|| to_trim_start(&arg)).unwrap_or(0);
            // collision is only an issue if the whole string is getting trimmed
            let trim_end = (self.right && trim_start != arg.len())
                .then(|| to_trim_end(&arg))
                .unwrap_or(0);

            ntrim += trim_start + trim_end;
            if !self.quiet {
                streams.out.append(&arg[trim_start..arg.len() - trim_end]);
                if want_newline {
                    streams.out.append1('\n');
                }
            } else if ntrim > 0 {
                return STATUS_CMD_OK;
            }
        }

        if ntrim > 0 {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        }
    }
}

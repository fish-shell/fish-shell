use super::*;

pub struct Trim {
    chars_to_trim: WString,
    left: bool,
    right: bool,
    quiet: bool,
}

impl Default for Trim {
    fn default() -> Self {
        Self {
            // from " \f\n\r\t\v"
            chars_to_trim: L!(" \x0C\n\r\x09\x0B").to_owned(),
            left: false,
            right: false,
            quiet: false,
        }
    }
}

impl StringSubCommand<'_> for Trim {
    fn long_options(&self) -> &'static [woption<'static>] {
        const opts: &'static [woption<'static>] = &[
            wopt(L!("chars"), required_argument, 'c'),
            wopt(L!("left"), no_argument, 'l'),
            wopt(L!("right"), no_argument, 'r'),
            wopt(L!("quiet"), no_argument, 'q'),
        ];
        opts
    }
    fn short_options(&self) -> &'static wstr {
        L!(":c:lrq")
    }

    fn parse_opt(&mut self, w: &mut wgetopter_t<'_, '_>, c: char) -> Result<(), StringError> {
        match c {
            'c' => self.chars_to_trim = w.woptarg().unwrap().to_owned(),
            'l' => self.left = true,
            'r' => self.right = true,
            'q' => self.quiet = true,
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

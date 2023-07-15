use super::*;

pub struct Transform {
    pub quiet: bool,
    pub func: fn(&wstr) -> WString,
}

impl StringSubCommand<'_> for Transform {
    fn long_options(&self) -> &'static [woption<'static>] {
        const opts: &'static [woption<'static>] = &[wopt(L!("quiet"), no_argument, 'q')];
        opts
    }
    fn short_options(&self) -> &'static wstr {
        L!(":q")
    }
    fn parse_opt(&mut self, w: &mut wgetopter_t<'_, '_>, c: char) -> Result<(), StringError> {
        match c {
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
        let mut n_transformed = 0usize;

        for (arg, want_newline) in arguments(args, optind, streams) {
            let transformed = (self.func)(&arg);
            if transformed != arg {
                n_transformed += 1;
            }
            if !self.quiet {
                streams.out.append(&transformed);
                if want_newline {
                    streams.out.append1('\n');
                }
            } else if n_transformed > 0 {
                return STATUS_CMD_OK;
            }
        }

        if n_transformed > 0 {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        }
    }
}

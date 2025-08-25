use super::*;

pub struct Transform {
    pub quiet: bool,
    pub func: fn(&wstr) -> WString,
}

impl StringSubCommand<'_> for Transform {
    const LONG_OPTIONS: &'static [WOption<'static>] = &[wopt(L!("quiet"), NoArgument, 'q')];
    const SHORT_OPTIONS: &'static wstr = L!("q");
    fn parse_opt(&mut self, _n: &wstr, c: char, _arg: Option<&wstr>) -> Result<(), StringError> {
        match c {
            'q' => self.quiet = true,
            _ => return Err(StringError::UnknownOption),
        }
        return Ok(());
    }

    fn handle(
        &mut self,
        _parser: &Parser,
        streams: &mut IoStreams,
        optind: &mut usize,
        args: &[&wstr],
    ) -> Result<(), ErrorCode> {
        let mut n_transformed = 0usize;

        for (arg, want_newline) in arguments(args, optind, streams) {
            let transformed = (self.func)(&arg);
            if transformed != arg {
                n_transformed += 1;
            }
            if !self.quiet {
                streams.out.append(transformed);
                if want_newline {
                    streams.out.append1('\n');
                }
            } else if n_transformed > 0 {
                return Ok(());
            }
        }

        if n_transformed > 0 {
            Ok(())
        } else {
            Err(STATUS_CMD_ERROR)
        }
    }
}

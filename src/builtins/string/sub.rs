use std::num::NonZeroI64;

use super::*;

#[derive(Default)]
pub struct Sub {
    length: Option<usize>,
    quiet: bool,
    start: Option<NonZeroI64>,
    end: Option<NonZeroI64>,
}

impl StringSubCommand<'_> for Sub {
    const LONG_OPTIONS: &'static [WOption<'static>] = &[
        wopt(L!("length"), RequiredArgument, 'l'),
        wopt(L!("start"), RequiredArgument, 's'),
        wopt(L!("end"), RequiredArgument, 'e'),
        wopt(L!("quiet"), NoArgument, 'q'),
    ];
    const SHORT_OPTIONS: &'static wstr = L!(":l:qs:e:");

    fn parse_opt(&mut self, name: &wstr, c: char, arg: Option<&wstr>) -> Result<(), StringError> {
        match c {
            'l' => {
                self.length =
                    Some(fish_wcstol(arg.unwrap())?.try_into().map_err(|_| {
                        invalid_args!("%ls: Invalid length value '%ls'\n", name, arg)
                    })?)
            }
            's' => {
                self.start =
                    Some(fish_wcstol(arg.unwrap())?.try_into().map_err(|_| {
                        invalid_args!("%ls: Invalid start value '%ls'\n", name, arg)
                    })?)
            }
            'e' => {
                self.end = Some(
                    fish_wcstol(arg.unwrap())?
                        .try_into()
                        .map_err(|_| invalid_args!("%ls: Invalid end value '%ls'\n", name, arg))?,
                )
            }
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
    ) -> Option<libc::c_int> {
        let cmd = args[0];
        if self.length.is_some() && self.end.is_some() {
            streams.err.append(wgettext_fmt!(
                BUILTIN_ERR_COMBO2,
                cmd,
                wgettext!("--end and --length are mutually exclusive")
            ));
            return STATUS_INVALID_ARGS;
        }

        let mut nsub = 0;
        for (s, want_newline) in arguments(args, optind, streams) {
            let start: usize = match self.start.map(i64::from).unwrap_or_default() {
                n @ 1.. => n as usize - 1,
                0 => 0,
                n => {
                    let n = u64::min(n.unsigned_abs(), usize::MAX as u64) as usize;
                    s.len().saturating_sub(n)
                }
            }
            .clamp(0, s.len());

            let count = {
                let n = self
                    .end
                    .map(|e| match i64::from(e) {
                        // end can never be 0
                        n @ 1.. => n as usize,
                        n => {
                            let n = u64::min(n.unsigned_abs(), usize::MAX as u64) as usize;
                            s.len().saturating_sub(n)
                        }
                    })
                    .map(|n| n.saturating_sub(start));

                self.length.or(n).unwrap_or(s.len())
            };

            if !self.quiet {
                streams
                    .out
                    .append(&s[start..usize::min(start + count, s.len())]);
                if want_newline {
                    streams.out.append1('\n');
                }
            }
            nsub += 1;
            if self.quiet {
                return STATUS_CMD_OK;
            }
        }

        if nsub > 0 {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        }
    }
}

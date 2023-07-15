use std::num::NonZeroI64;

use super::*;
use crate::wutil::fish_wcstol;

#[derive(Default)]
pub struct Sub {
    length: Option<usize>,
    quiet: bool,
    start: Option<NonZeroI64>,
    end: Option<NonZeroI64>,
}

impl StringSubCommand<'_> for Sub {
    fn long_options(&self) -> &'static [woption<'static>] {
        const opts: &'static [woption<'static>] = &[
            wopt(L!("length"), required_argument, 'l'),
            wopt(L!("start"), required_argument, 's'),
            wopt(L!("end"), required_argument, 'e'),
            wopt(L!("quiet"), no_argument, 'q'),
        ];
        opts
    }
    fn short_options(&self) -> &'static wstr {
        L!(":l:qs:e:")
    }

    fn parse_opt(&mut self, w: &mut wgetopter_t<'_, '_>, c: char) -> Result<(), StringError> {
        match c {
            'l' => {
                self.length = Some(fish_wcstol(w.woptarg().unwrap())?.try_into().map_err(|_| {
                    invalid_args!("%ls: Invalid length value '%ls'\n", w.cmd(), w.woptarg())
                })?)
            }
            's' => {
                self.start = Some(fish_wcstol(w.woptarg().unwrap())?.try_into().map_err(|_| {
                    invalid_args!("%ls: Invalid start value '%ls'\n", w.cmd(), w.woptarg())
                })?)
            }
            'e' => {
                self.end = Some(fish_wcstol(w.woptarg().unwrap())?.try_into().map_err(|_| {
                    invalid_args!("%ls: Invalid end value '%ls'\n", w.cmd(), w.woptarg())
                })?)
            }
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
        let cmd = &args[0];
        if self.length.is_some() && self.end.is_some() {
            streams.err.append(&wgettext_fmt!(
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

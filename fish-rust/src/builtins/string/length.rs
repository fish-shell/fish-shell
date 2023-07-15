use super::*;

use crate::wcstringutil::split_string;

#[derive(Default)]
pub struct Length {
    quiet: bool,
    visible: bool,
}

impl StringSubCommand<'_> for Length {
    fn long_options(&self) -> &'static [woption<'static>] {
        const opts: &'static [woption<'static>] = &[
            wopt(L!("quiet"), no_argument, 'q'),
            wopt(L!("visible"), no_argument, 'V'),
        ];
        opts
    }
    fn short_options(&self) -> &'static wstr {
        L!(":qV")
    }

    fn parse_opt(&mut self, w: &mut wgetopter_t<'_, '_>, c: char) -> Result<(), StringError> {
        match c {
            'q' => self.quiet = true,
            'V' => self.visible = true,
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
        let mut nnonempty = 0usize;

        for (arg, _) in arguments(args, optind, streams) {
            if self.visible {
                // Visible length only makes sense line-wise.
                for line in split_string(&arg, '\n') {
                    let mut max = 0;
                    // Carriage-return returns us to the beginning. The longest substring without
                    // carriage-return determines the overall width.
                    for reset in split_string(&line, '\r') {
                        let n = width_without_escapes(&reset, 0);
                        max = usize::max(max, n);
                    }
                    if max > 0 {
                        nnonempty += 1;
                    }
                    if !self.quiet {
                        streams.out.appendln_owned(max.to_wstring());
                    } else if nnonempty > 0 {
                        return STATUS_CMD_OK;
                    }
                }
            } else {
                let n = arg.len();
                if n > 0 {
                    nnonempty += 1;
                }
                if !self.quiet {
                    streams.out.appendln_owned(n.to_wstring());
                } else if nnonempty > 0 {
                    return STATUS_CMD_OK;
                }
            }
        }
        if nnonempty > 0 {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        }
    }
}

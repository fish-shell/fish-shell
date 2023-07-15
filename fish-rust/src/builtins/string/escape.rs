use super::*;
use crate::common::{escape_string, EscapeFlags, EscapeStringStyle};

#[derive(Default)]
pub struct Escape {
    no_quoted: bool,
    style: EscapeStringStyle,
}

impl StringSubCommand<'_> for Escape {
    fn long_options(&self) -> &'static [woption<'static>] {
        const opts: &'static [woption<'static>] = &[
            wopt(L!("no-quoted"), no_argument, 'n'),
            wopt(L!("style"), required_argument, NONOPTION_CHAR_CODE),
        ];
        opts
    }
    fn short_options(&self) -> &'static wstr {
        L!(":n")
    }

    fn parse_opt(&mut self, w: &mut wgetopter_t<'_, '_>, c: char) -> Result<(), StringError> {
        match c {
            'n' => self.no_quoted = true,
            NONOPTION_CHAR_CODE => {
                self.style = w.woptarg().unwrap().try_into().map_err(|_| {
                    invalid_args!("%ls: Invalid escape style '%ls'\n", w.cmd(), w.woptarg())
                })?
            }
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
        // Currently, only the script style supports options.
        // Ignore them for other styles for now.
        let style = match self.style {
            EscapeStringStyle::Script(..) if self.no_quoted => {
                EscapeStringStyle::Script(EscapeFlags::NO_QUOTED)
            }
            x => x,
        };

        let mut escaped_any = false;
        for (arg, want_newline) in arguments(args, optind, streams) {
            let mut escaped = escape_string(&arg, style);

            if want_newline {
                escaped.push('\n');
            }

            streams.out.appendln_owned(escaped);
            escaped_any = true;
        }

        if escaped_any {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        }
    }
}

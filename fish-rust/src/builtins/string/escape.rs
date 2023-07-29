use super::*;
use crate::common::{escape_string, EscapeFlags, EscapeStringStyle};

#[derive(Default)]
pub struct Escape {
    no_quoted: bool,
    style: EscapeStringStyle,
}

impl StringSubCommand<'_> for Escape {
    const LONG_OPTIONS: &'static [woption<'static>] = &[
        wopt(L!("no-quoted"), no_argument, 'n'),
        wopt(L!("style"), required_argument, NONOPTION_CHAR_CODE),
    ];
    const SHORT_OPTIONS: &'static wstr = L!(":n");

    fn parse_opt(&mut self, name: &wstr, c: char, arg: Option<&wstr>) -> Result<(), StringError> {
        match c {
            'n' => self.no_quoted = true,
            NONOPTION_CHAR_CODE => {
                self.style = arg
                    .unwrap()
                    .try_into()
                    .map_err(|_| invalid_args!("%ls: Invalid escape style '%ls'\n", name, arg))?
            }
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

            streams.out.append(escaped);
            escaped_any = true;
        }

        if escaped_any {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        }
    }
}

use super::*;
use crate::common::{unescape_string, UnescapeStringStyle};

#[derive(Default)]
pub struct Unescape {
    no_quoted: bool,
    style: UnescapeStringStyle,
}

impl StringSubCommand<'_> for Unescape {
    const LONG_OPTIONS: &'static [woption<'static>] = &[
        // FIXME: this flag means nothing, but was present in the C++ code
        // should be removed
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
                    .map_err(|_| invalid_args!("%ls: Invalid style value '%ls'\n", name, arg))?
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
        let mut nesc = 0;
        for (arg, want_newline) in arguments(args, optind, streams) {
            if let Some(res) = unescape_string(&arg, self.style) {
                streams.out.append(res);
                if want_newline {
                    streams.out.append1('\n');
                }
                nesc += 1;
            }
        }

        if nesc > 0 {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        }
    }
}

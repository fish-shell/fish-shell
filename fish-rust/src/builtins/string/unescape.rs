use super::*;
use crate::common::{unescape_string, UnescapeStringStyle};

#[derive(Default)]
pub struct Unescape {
    no_quoted: bool,
    style: UnescapeStringStyle,
}

impl StringSubCommand<'_> for Unescape {
    fn long_options(&self) -> &'static [woption<'static>] {
        const opts: &'static [woption<'static>] = &[
            // FIXME: this flag means nothing, but was present in the C++ code
            // should be removed
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
                    invalid_args!("%ls: Invalid style value '%ls'\n", w.cmd(), w.woptarg())
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
        let mut nesc = 0;
        for (arg, want_newline) in arguments(args, optind, streams) {
            if let Some(res) = unescape_string(&arg, self.style) {
                streams.out.append(&res);
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

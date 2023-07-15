use std::borrow::Cow;

use super::*;
use crate::fallback::fish_wcwidth;
use crate::wutil::fish_wcstol;

pub struct Pad {
    char_to_pad: char,
    pad_char_width: usize,
    pad_from: Direction,
    width: usize,
}

impl Default for Pad {
    fn default() -> Self {
        Self {
            char_to_pad: ' ',
            pad_char_width: 1,
            pad_from: Direction::Left,
            width: 0,
        }
    }
}

impl StringSubCommand<'_> for Pad {
    fn long_options(&self) -> &'static [woption<'static>] {
        const opts: &'static [woption<'static>] = &[
            // FIXME docs say `--char`, there was no long_opt with `--char` in C++
            wopt(L!("chars"), required_argument, 'c'),
            wopt(L!("right"), no_argument, 'r'),
            wopt(L!("width"), required_argument, 'w'),
        ];
        opts
    }
    fn short_options(&self) -> &'static wstr {
        L!(":c:rw:")
    }

    fn parse_opt(&mut self, w: &mut wgetopter_t<'_, '_>, c: char) -> Result<(), StringError> {
        match c {
            'c' => {
                let [pad_char] = w.woptarg().unwrap().as_char_slice() else {
                    return Err(invalid_args!(
                        "%ls: Padding should be a character '%ls'\n",
                        w.cmd(),
                        w.woptarg()
                    ));
                };
                let pad_char_width = fish_wcwidth(*pad_char);
                if pad_char_width <= 0 {
                    return Err(invalid_args!(
                        "%ls: Invalid padding character of width zero '%ls'\n",
                        w.cmd(),
                        w.woptarg()
                    ));
                }
                self.pad_char_width = pad_char_width as usize;
                self.char_to_pad = *pad_char;
            }
            'r' => self.pad_from = Direction::Right,
            'w' => {
                self.width = fish_wcstol(w.woptarg().unwrap())?.try_into().map_err(|_| {
                    invalid_args!("%ls: Invalid width value '%ls'\n", w.cmd(), w.woptarg())
                })?
            }
            _ => return Err(StringError::UnknownOption),
        }
        return Ok(());
    }

    fn handle<'args>(
        &mut self,
        _parser: &Parser,
        streams: &mut IoStreams<'_>,
        optind: &mut usize,
        args: &'args [WString],
    ) -> Option<libc::c_int> {
        let mut max_width = 0usize;
        let mut inputs: Vec<(WString, usize)> = Vec::new();
        let mut print_trailing_newline = true;

        for (arg, want_newline) in arguments(args, optind, streams) {
            let width = width_without_escapes(&arg, 0);
            max_width = max_width.max(width);
            inputs.push((arg, width));
            print_trailing_newline = want_newline;
        }

        let pad_width = max_width.max(self.width);

        for (input, width) in inputs {
            use std::iter::repeat;

            let pad = (pad_width - width) / self.pad_char_width;
            let remaining_width = (pad_width - width) % self.pad_char_width;
            let mut padded: WString = match self.pad_from {
                Direction::Left => repeat(self.char_to_pad)
                    .take(pad)
                    .chain(repeat(' ').take(remaining_width))
                    .chain(input.chars())
                    .collect(),
                Direction::Right => input
                    .chars()
                    .chain(repeat(' ').take(remaining_width))
                    .chain(repeat(self.char_to_pad).take(pad))
                    .collect(),
            };

            if print_trailing_newline {
                padded.push('\n');
            }

            streams.out.append(&padded);
        }

        STATUS_CMD_OK
    }
}

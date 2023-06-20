use std::borrow::Cow;

use super::*;
use crate::wutil::{fish_wcstol, fish_wcswidth};

pub struct Pad {
    char_to_pad: char,
    pad_char_width: i32,
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
    const LONG_OPTIONS: &'static [woption<'static>] = &[
        // FIXME docs say `--char`, there was no long_opt with `--char` in C++
        wopt(L!("chars"), required_argument, 'c'),
        wopt(L!("right"), no_argument, 'r'),
        wopt(L!("width"), required_argument, 'w'),
    ];
    const SHORT_OPTIONS: &'static wstr = L!(":c:rw:");

    fn parse_opt(&mut self, name: &wstr, c: char, arg: Option<&wstr>) -> Result<(), StringError> {
        match c {
            'c' => {
                let arg = arg.expect("option -c requires an argument");
                if arg.len() != 1 {
                    return Err(invalid_args!(
                        "%ls: Padding should be a character '%ls'\n",
                        name,
                        Some(arg)
                    ));
                }
                let pad_char_width = fish_wcswidth(arg.slice_to(1));
                // can we ever have negative width?
                if pad_char_width == 0 {
                    return Err(invalid_args!(
                        "%ls: Invalid padding character of width zero '%ls'\n",
                        name,
                        Some(arg)
                    ));
                }
                self.pad_char_width = pad_char_width;
                self.char_to_pad = arg.char_at(0);
            }
            'r' => self.pad_from = Direction::Right,
            'w' => {
                self.width = fish_wcstol(arg.unwrap())?
                    .try_into()
                    .map_err(|_| invalid_args!("%ls: Invalid width value '%ls'\n", name, arg))?
            }
            _ => return Err(StringError::UnknownOption),
        }
        return Ok(());
    }

    fn handle<'args>(
        &mut self,
        _parser: &mut parser_t,
        streams: &mut io_streams_t,
        optind: &mut usize,
        args: &[&'args wstr],
    ) -> Option<libc::c_int> {
        let mut max_width = 0i32;
        let mut inputs: Vec<(Cow<'args, wstr>, i32)> = Vec::new();
        let mut print_newline = true;

        for (arg, want_newline) in Arguments::new(args, optind, streams) {
            let width = width_without_escapes(&arg, 0);
            max_width = max_width.max(width);
            inputs.push((arg, width));
            print_newline = want_newline;
        }

        let pad_width = max_width.max(self.width as i32);

        for (input, width) in inputs {
            use std::iter::repeat;

            let pad = (pad_width - width) / self.pad_char_width;
            let remaining_width = (pad_width - width) % self.pad_char_width;
            let mut padded: WString = match self.pad_from {
                Direction::Left => repeat(self.char_to_pad)
                    .take(pad as usize)
                    .chain(repeat(' ').take(remaining_width as usize))
                    .chain(input.chars())
                    .collect(),
                Direction::Right => input
                    .chars()
                    .chain(repeat(' ').take(remaining_width as usize))
                    .chain(repeat(self.char_to_pad).take(pad as usize))
                    .collect(),
            };

            if print_newline {
                padded.push('\n');
            }

            streams.out.append(padded);
        }

        STATUS_CMD_OK
    }
}

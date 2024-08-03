use super::*;
use crate::common::get_ellipsis_str;

pub struct Shorten<'args> {
    ellipsis: &'args wstr,
    ellipsis_width: usize,
    max: Option<usize>,
    no_newline: bool,
    quiet: bool,
    shorten_from: Direction,
}

impl Default for Shorten<'_> {
    fn default() -> Self {
        Self {
            ellipsis: get_ellipsis_str(),
            ellipsis_width: width_without_escapes(get_ellipsis_str(), 0),
            max: None,
            no_newline: false,
            quiet: false,
            shorten_from: Direction::Right,
        }
    }
}

impl<'args> StringSubCommand<'args> for Shorten<'args> {
    const LONG_OPTIONS: &'static [WOption<'static>] = &[
        // FIXME: documentation says it's --char
        wopt(L!("chars"), RequiredArgument, 'c'),
        wopt(L!("max"), RequiredArgument, 'm'),
        wopt(L!("no-newline"), NoArgument, 'N'),
        wopt(L!("left"), NoArgument, 'l'),
        wopt(L!("quiet"), NoArgument, 'q'),
    ];
    const SHORT_OPTIONS: &'static wstr = L!(":c:m:Nlq");

    fn parse_opt(
        &mut self,
        name: &wstr,
        c: char,
        arg: Option<&'args wstr>,
    ) -> Result<(), StringError> {
        match c {
            'c' => {
                self.ellipsis = arg.unwrap();
                self.ellipsis_width = width_without_escapes(self.ellipsis, 0);
            }
            'm' => {
                self.max = Some(
                    fish_wcstol(arg.unwrap())?
                        .try_into()
                        .map_err(|_| invalid_args!("%ls: Invalid max value '%ls'\n", name, arg))?,
                )
            }
            'N' => self.no_newline = true,
            'l' => self.shorten_from = Direction::Left,
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
        let mut min_width = usize::MAX;
        let mut inputs = Vec::new();

        let iter = arguments(args, optind, streams);

        if self.max == Some(0) {
            // Special case: Max of 0 means no shortening.
            // This makes this more reusable, so you don't need special-cases like
            //
            // if test $shorten -gt 0
            //     string shorten -m $shorten whatever
            // else
            //     echo whatever
            // end
            for (arg, _) in iter {
                streams.out.appendln(arg);
            }
            return STATUS_CMD_OK;
        }

        for (arg, _) in iter {
            // Visible width only makes sense line-wise.
            // So either we have no-newlines (which means we shorten on the first newline),
            // or we handle the lines separately.
            let mut splits = arg.split('\n').peekable();
            if self.no_newline && splits.peek().is_some() {
                let mut s = match self.shorten_from {
                    Direction::Right => splits.next(),
                    Direction::Left => splits.last(),
                }
                .unwrap()
                .to_owned();
                s.push_utfstr(&self.ellipsis);
                let width = width_without_escapes(&s, 0);

                if width > 0 && width < min_width {
                    min_width = width;
                }
                inputs.push(s);
            } else {
                for s in splits {
                    let width = width_without_escapes(s, 0);
                    if width > 0 && width < min_width {
                        min_width = width;
                    }
                    inputs.push(s.into());
                }
            }
        }

        let ourmax: usize = self.max.unwrap_or(min_width);

        let (ell, ell_width) = if self.ellipsis_width > ourmax {
            // If we can't even print our ellipsis, we substitute nothing,
            // truncating instead.
            (L!(""), 0)
        } else {
            (self.ellipsis, self.ellipsis_width)
        };

        let mut nsub = 0usize;
        // We could also error out here if the width of our ellipsis is larger
        // than the target width.
        // That seems excessive - specifically because the ellipsis on LANG=C
        // is "..." (width 3!).

        let skip_escapes = |l: &wstr, pos: usize| -> usize {
            let mut totallen = 0usize;
            while l.char_at(pos + totallen) == '\x1B' {
                let Some(len) = escape_code_length(l.slice_from(pos + totallen)) else {
                    break;
                };
                totallen += len;
            }
            totallen
        };

        for line in inputs {
            let mut pos = 0usize;
            let mut max = 0usize;
            // Collect how much of the string we can use without going over the maximum.
            if self.shorten_from == Direction::Left {
                // Our strategy for keeping from the end.
                // This is rather unoptimized - actually going *backwards* from the end
                // is extremely tricky because we would have to subtract escapes again.
                // Also we need to avoid hacking combiners into bits.
                // This should work for most cases considering the combiners typically have width 0.
                let mut out = L!("");
                while pos < line.len() {
                    let w = width_without_escapes(&line, pos);
                    // If we're at the beginning and it fits, we sits.
                    //
                    // Otherwise we require it to fit the ellipsis
                    if (w <= ourmax && pos == 0) || (w + ell_width <= ourmax) {
                        out = line.slice_from(pos);
                        break;
                    }

                    pos += skip_escapes(&line, pos).max(1);
                }
                if self.quiet && pos != 0 {
                    return STATUS_CMD_OK;
                }

                let output = match pos {
                    0 => line,
                    _ => {
                        // We have an ellipsis, construct our string and print it.
                        nsub += 1;
                        let mut res = WString::with_capacity(ell.len() + out.len());
                        res.push_utfstr(ell);
                        res.push_utfstr(out);
                        res
                    }
                };
                streams.out.appendln(output);
                continue;
            } else {
                /* shorten the right side */
                // Going from the left.
                // This is somewhat easier.
                while max <= ourmax && pos < line.len() {
                    pos += skip_escapes(&line, pos);
                    let w = fish_wcwidth_visible(line.char_at(pos));
                    if w <= 0 || max + w as usize + ell_width <= ourmax {
                        // If it still fits, even if it is the last, we add it.
                        max = max.saturating_add_signed(w);
                        pos += 1;
                    } else {
                        // We're at the limit, so see if the entire string fits.
                        let mut max2 = max + w as usize;
                        let mut pos2 = pos + 1;
                        while pos2 < line.len() {
                            pos2 += skip_escapes(&line, pos2);
                            let w = fish_wcwidth_visible(line.char_at(pos2));
                            max2 = max2.saturating_add_signed(w);
                            pos2 += 1;
                        }

                        if max2 <= ourmax {
                            // We're at the end and everything fits,
                            // no ellipsis.
                            pos = pos2;
                        }
                        break;
                    }
                }
            }

            if self.quiet && pos != line.len() {
                return STATUS_CMD_OK;
            }

            if pos == line.len() {
                streams.out.appendln(line);
                continue;
            }

            nsub += 1;
            let mut newl = line;
            newl.truncate(pos);
            newl.push_utfstr(ell);
            newl.push('\n');
            streams.out.append(newl);
        }

        // Return true if we have shortened something and false otherwise.
        if nsub > 0 {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        }
    }
}

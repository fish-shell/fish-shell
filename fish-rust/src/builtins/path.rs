use crate::env::environment::Environment;
use std::cmp::Ordering;
use std::os::unix::prelude::{FileTypeExt, MetadataExt};
use std::time::SystemTime;

use super::prelude::*;
use crate::path::path_apply_working_directory;
use crate::util::wcsfilecmp_glob;
use crate::wcstringutil::split_string_tok;
use crate::wutil::{
    file_id_for_path, lwstat, normalize_path, waccess, wbasename, wdirname, wrealpath, wstat,
    INVALID_FILE_ID,
};
use bitflags::bitflags;
use libc::{getegid, geteuid, mode_t, uid_t, F_OK, PATH_MAX, R_OK, S_ISGID, S_ISUID, W_OK, X_OK};

use super::shared::BuiltinCmd;

macro_rules! path_error {
    (
    $streams:expr,
    $string:expr
    $(, $args:expr)+
    $(,)?
    ) => {
        $streams.err.append(L!("path "));
        $streams.err.append(&wgettext_fmt!($string, $($args),*));
    };
}

fn path_unknown_option(parser: &Parser, streams: &mut IoStreams<'_>, subcmd: &wstr, opt: &wstr) {
    path_error!(streams, BUILTIN_ERR_UNKNOWN, subcmd, opt);
    builtin_print_error_trailer(parser, streams.err, L!("path"));
}

// How many bytes we read() at once.
// We use PATH_MAX here so we always get at least one path,
// and so we can automatically detect NULL-separated input.
const PATH_CHUNK_SIZE: usize = PATH_MAX as usize;
#[inline]
fn arguments<'iter>(
    args: &'iter [WString],
    optind: &'iter mut usize,
    streams: &mut IoStreams<'_>,
) -> Arguments<'iter> {
    Arguments::new(args, optind, streams, PATH_CHUNK_SIZE)
}

bitflags! {
    #[derive(Default)]
    pub struct TypeFlags: u32 {
        /// A block device
        const BLOCK = 1 << 0;
        /// A directory
        const DIR = 1 << 1;
        /// A regular file
        const FILE = 1 << 2;
        /// A link
        const LINK = 1 << 3;
        /// A character device
        const CHAR = 1 << 4;
        /// A fifo
        const FIFO = 1 << 5;
        /// A socket
        const SOCK = 1 << 6;
    }
}

impl TryFrom<&wstr> for TypeFlags {
    type Error = ();

    fn try_from(value: &wstr) -> Result<Self, Self::Error> {
        let flag = match value {
            t if t == "file" => Self::FILE,
            t if t == "dir" => Self::DIR,
            t if t == "block" => Self::BLOCK,
            t if t == "char" => Self::CHAR,
            t if t == "fifo" => Self::FIFO,
            t if t == "socket" => Self::SOCK,
            t if t == "link" => Self::LINK,
            _ => return Err(()),
        };

        Ok(flag)
    }
}

bitflags! {
    #[derive(Default)]
    pub struct PermFlags: u32 {
        const READ = 1 << 0;
        const WRITE = 1 << 1;
        const EXEC = 1 << 2;
        const SUID = 1 << 3;
        const SGID = 1 << 4;
        const USER = 1 << 5;
        const GROUP = 1 << 6;
    }
}

impl PermFlags {
    fn is_special(self) -> bool {
        self.intersects(Self::SUID | Self::SGID | Self::USER | Self::GROUP)
    }
}

impl TryFrom<&wstr> for PermFlags {
    type Error = ();

    fn try_from(value: &wstr) -> Result<Self, Self::Error> {
        let flag = match value {
            t if t == "read" => Self::READ,
            t if t == "write" => Self::WRITE,
            t if t == "exec" => Self::EXEC,
            t if t == "suid" => Self::SUID,
            t if t == "sgid" => Self::SGID,
            t if t == "user" => Self::USER,
            t if t == "group" => Self::GROUP,
            _ => return Err(()),
        };

        Ok(flag)
    }
}

/// This is used by the subcommands to communicate with the option parser which flags are
/// valid and get the result of parsing the command for flags.
#[derive(Default)]
struct Options {
    null_in: bool,
    null_out: bool,
    quiet: bool,

    invert_valid: bool,
    invert: bool,

    relative_valid: bool,
    relative: bool,

    reverse_valid: bool,
    reverse: bool,

    unique_valid: bool,
    unique: bool,

    key: Option<WString>,

    types_valid: bool,
    types: Option<TypeFlags>,

    perms_valid: bool,
    perms: Option<PermFlags>,

    arg1: Option<WString>,
}

#[inline]
fn path_out(streams: &mut IoStreams<'_>, opts: &Options, s: impl AsRef<wstr>) {
    let s = s.as_ref();
    if !opts.quiet {
        if !opts.null_out {
            streams
                .out
                .append_with_separation(s, SeparationType::explicitly, true);
        } else {
            let mut output = WString::with_capacity(s.len() + 1);
            output.push_utfstr(s);
            output.push('\0');
            streams.out.append(&output);
        }
    }
}

fn construct_short_opts(opts: &Options) -> WString {
    // All commands accept -z, -Z and -q
    let mut short_opts = WString::from(":zZq");
    if opts.perms_valid {
        short_opts += L!("p:");
        short_opts += L!("rwx");
    }

    if opts.types_valid {
        short_opts += L!("t:");
        short_opts += L!("fld");
    }

    if opts.invert_valid {
        short_opts.push('v');
    }
    if opts.relative_valid {
        short_opts.push('R');
    }
    if opts.reverse_valid {
        short_opts.push('r');
    }
    if opts.unique_valid {
        short_opts.push('u');
    }

    short_opts
}

/// Note that several long flags share the same short flag. That is okay. The caller is expected
/// to indicate that a max of one of the long flags sharing a short flag is valid.
/// Remember: adjust the completions in share/completions/ when options change
const LONG_OPTIONS: [woption<'static>; 10] = [
    wopt(L!("quiet"), no_argument, 'q'),
    wopt(L!("null-in"), no_argument, 'z'),
    wopt(L!("null-out"), no_argument, 'Z'),
    wopt(L!("perm"), required_argument, 'p'),
    wopt(L!("type"), required_argument, 't'),
    wopt(L!("invert"), no_argument, 'v'),
    wopt(L!("relative"), no_argument, 'R'),
    wopt(L!("reverse"), no_argument, 'r'),
    wopt(L!("unique"), no_argument, 'u'),
    wopt(L!("key"), required_argument, NONOPTION_CHAR_CODE),
];

fn parse_opts(
    opts: &mut Options,
    optind: &mut usize,
    n_req_args: usize,
    args: &mut [WString],
    parser: &Parser,
    streams: &mut IoStreams<'_>,
) -> Option<c_int> {
    let mut args_read = Vec::with_capacity(args.len());
    args_read.extend_from_slice(args);

    let short_opts = construct_short_opts(opts);

    let mut w = wgetopter_t::new(&short_opts, &LONG_OPTIONS, args);
    while let Some(c) = w.wgetopt_long() {
        match c {
            ':' => {
                streams.err.append(L!("path ")); // clone of string_error
                builtin_missing_argument(
                    parser,
                    streams,
                    w.cmd(),
                    &args_read[w.woptind - 1],
                    false,
                );
                return STATUS_INVALID_ARGS;
            }
            '?' => {
                path_unknown_option(parser, streams, w.cmd(), &args_read[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            'q' => {
                opts.quiet = true;
                continue;
            }
            'z' => {
                opts.null_in = true;
                continue;
            }
            'Z' => {
                opts.null_out = true;
                continue;
            }
            'v' if opts.invert_valid => {
                opts.invert = true;
                continue;
            }
            't' if opts.types_valid => {
                let types = opts.types.get_or_insert_with(TypeFlags::default);
                let types_args = split_string_tok(w.woptarg().unwrap(), L!(","), None);
                for t in types_args {
                    let Ok(r#type) = t.try_into() else {
                        path_error!(streams, "%ls: Invalid type '%ls'\n", "path", t);
                        return STATUS_INVALID_ARGS;
                    };
                    *types |= r#type;
                }
                continue;
            }
            'p' if opts.perms_valid => {
                let perms = opts.perms.get_or_insert_with(PermFlags::default);
                let perms_args = split_string_tok(w.woptarg().unwrap(), L!(","), None);
                for p in perms_args {
                    let Ok(perm) = p.try_into() else {
                        path_error!(streams, "%ls: Invalid permission '%ls'\n", "path", p);
                        return STATUS_INVALID_ARGS;
                    };
                    *perms |= perm;
                }
                continue;
            }
            'r' if opts.reverse_valid => {
                opts.reverse = true;
                continue;
            }
            'r' if opts.perms_valid => {
                let perms = opts.perms.get_or_insert_with(PermFlags::default);
                *perms |= PermFlags::READ;
                continue;
            }
            'w' if opts.perms_valid => {
                let perms = opts.perms.get_or_insert_with(PermFlags::default);
                *perms |= PermFlags::WRITE;
                continue;
            }
            'x' if opts.perms_valid => {
                let perms = opts.perms.get_or_insert_with(PermFlags::default);
                *perms |= PermFlags::EXEC;
                continue;
            }
            'f' if opts.types_valid => {
                let types = opts.types.get_or_insert_with(TypeFlags::default);
                *types |= TypeFlags::FILE;
                continue;
            }
            'l' if opts.types_valid => {
                let types = opts.types.get_or_insert_with(TypeFlags::default);
                *types |= TypeFlags::LINK;
                continue;
            }
            'd' if opts.types_valid => {
                let types = opts.types.get_or_insert_with(TypeFlags::default);
                *types |= TypeFlags::DIR;
                continue;
            }
            'u' if opts.unique_valid => {
                opts.unique = true;
                continue;
            }
            'R' if opts.relative_valid => {
                opts.relative = true;
                continue;
            }
            NONOPTION_CHAR_CODE => {
                assert!(w.woptarg().is_some());
                opts.key = w.woptarg().map(|s| s.to_owned());
                continue;
            }
            _ => {
                path_unknown_option(parser, streams, w.cmd(), &args_read[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
        }
    }

    *optind = w.woptind;

    if n_req_args != 0 {
        assert!(n_req_args == 1);
        opts.arg1 = w.argv().get(*optind).map(|s| s.to_owned());
        if opts.arg1.is_some() {
            *optind += 1;
        }

        if opts.arg1.is_none() && n_req_args == 1 {
            path_error!(streams, BUILTIN_ERR_ARG_COUNT0, w.cmd());
            return STATUS_INVALID_ARGS;
        }
    }

    // At this point we should not have optional args and be reading args from stdin.
    if streams.stdin_is_directly_redirected && w.argv().len() > *optind {
        path_error!(streams, BUILTIN_ERR_TOO_MANY_ARGUMENTS, w.cmd());
        return STATUS_INVALID_ARGS;
    }

    STATUS_CMD_OK
}

fn path_transform(
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
    func: impl Fn(&wstr) -> WString,
) -> Option<c_int> {
    let mut opts = Options::default();
    let mut optind = 0;
    let retval = parse_opts(&mut opts, &mut optind, 0, args, parser, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }

    let mut n_transformed = 0;
    let arguments = arguments(args, &mut optind, streams).with_split_behavior(match opts.null_in {
        true => SplitBehavior::Null,
        false => SplitBehavior::InferNull,
    });
    for (arg, _) in arguments {
        // Empty paths make no sense, but e.g. wbasename returns true for them.
        if arg.is_empty() {
            continue;
        }
        let transformed = func(&arg);
        if transformed != arg {
            n_transformed += 1;
            // Return okay if path wasn't already in this form
            // TODO: Is that correct?
            if opts.quiet {
                return STATUS_CMD_OK;
            };
        }
        path_out(streams, &opts, transformed);
    }

    if n_transformed > 0 {
        STATUS_CMD_OK
    } else {
        STATUS_CMD_ERROR
    }
}

fn path_basename(
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
) -> Option<c_int> {
    path_transform(parser, streams, args, |s| wbasename(s).to_owned())
}

fn path_dirname(
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
) -> Option<c_int> {
    path_transform(parser, streams, args, |s| wdirname(s).to_owned())
}

fn normalize_help(path: &wstr) -> WString {
    let mut np = normalize_path(path, false);
    if !np.is_empty() && np.char_at(0) == '-' {
        np = "./".chars().chain(np.chars()).collect();
    }
    np
}

fn path_normalize(
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
) -> Option<c_int> {
    path_transform(parser, streams, args, normalize_help)
}

fn path_mtime(parser: &Parser, streams: &mut IoStreams<'_>, args: &mut [WString]) -> Option<c_int> {
    let mut opts = Options::default();
    opts.relative_valid = true;
    let mut optind = 0;
    let retval = parse_opts(&mut opts, &mut optind, 0, args, parser, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }

    let mut n_transformed = 0;

    let t = match SystemTime::now().duration_since(SystemTime::UNIX_EPOCH) {
        Ok(dur) => dur.as_secs() as i64,
        Err(err) => -(err.duration().as_secs() as i64),
    };

    let arguments = arguments(args, &mut optind, streams).with_split_behavior(match opts.null_in {
        true => SplitBehavior::Null,
        false => SplitBehavior::InferNull,
    });
    for (arg, _) in arguments {
        let ret = file_id_for_path(&arg);

        if ret != INVALID_FILE_ID {
            if opts.quiet {
                return STATUS_CMD_OK;
            }
            n_transformed += 1;
            if !opts.relative {
                path_out(streams, &opts, (ret.mod_seconds).to_wstring());
            } else {
                // note that the mod time can actually be before the system time
                // so this can end up negative
                #[allow(clippy::unnecessary_cast)]
                path_out(streams, &opts, (t - ret.mod_seconds as i64).to_wstring());
            }
        }
    }

    if n_transformed > 0 {
        STATUS_CMD_OK
    } else {
        STATUS_CMD_ERROR
    }
}

fn find_extension(path: &wstr) -> Option<usize> {
    // The extension belongs to the basename,
    // if there is a "." before the last component it doesn't matter.
    // e.g. ~/.config/fish/conf.d/foo
    // does not have an extension! The ".d" here is not a file extension for "foo".
    // And "~/.config" doesn't have an extension either - the ".config" is the filename.
    let filename = wbasename(path);

    // "." and ".." aren't really *files* and therefore don't have an extension.
    if filename == "." || filename == ".." {
        return None;
    }

    // If we don't have a "." or the "." is the first in the filename,
    // we do not have an extension
    let pos = filename.chars().rposition(|c| c == '.');
    match pos {
        None | Some(0) => None,
        // Convert pos back to what it would be in the original path.
        Some(pos) => Some(pos + path.len() - filename.len()),
    }
}

#[test]
fn test_find_extension() {
    let cases = [
        (L!("foo.wmv"), Some(3)),
        (L!("verylongfilename.wmv"), Some("verylongfilename".len())),
        (L!("foo"), None),
        (L!(".foo"), None),
        (L!("./foo.wmv"), Some(5)),
    ];

    for (f, ext_idx) in cases {
        assert_eq!(find_extension(f), ext_idx);
    }
}

fn path_extension(
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
) -> Option<c_int> {
    let mut opts = Options::default();
    let mut optind = 0;
    let retval = parse_opts(&mut opts, &mut optind, 0, args, parser, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }

    let mut n_transformed = 0;
    let arguments = arguments(args, &mut optind, streams).with_split_behavior(match opts.null_in {
        true => SplitBehavior::Null,
        false => SplitBehavior::InferNull,
    });
    for (arg, _) in arguments {
        let pos = find_extension(&arg);
        let Some(pos) = pos else {
            // If there is no extension the extension is empty.
            // This is unambiguous because we include the ".".
            path_out(streams, &opts, L!(""));
            continue;
        };

        let ext = arg.slice_from(pos);
        if opts.quiet && !ext.is_empty() {
            return STATUS_CMD_OK;
        }
        path_out(streams, &opts, ext);
        n_transformed += 1;
    }

    if n_transformed > 0 {
        STATUS_CMD_OK
    } else {
        STATUS_CMD_ERROR
    }
}

fn path_change_extension(
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
) -> Option<c_int> {
    let mut opts = Options::default();
    let mut optind = 0;
    let retval = parse_opts(&mut opts, &mut optind, 1, args, parser, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }

    let mut n_transformed = 0usize;
    let arguments = arguments(args, &mut optind, streams).with_split_behavior(match opts.null_in {
        true => SplitBehavior::Null,
        false => SplitBehavior::InferNull,
    });
    for (mut arg, _) in arguments {
        let pos = find_extension(&arg);
        let mut ext = match pos {
            Some(pos) => {
                arg.truncate(pos);
                arg
            }
            None => arg,
        };

        // Only add on the extension "." if we have something.
        // That way specifying an empty extension strips it.
        if let Some(replacement) = opts.arg1.as_ref() {
            if !replacement.is_empty() {
                if replacement.char_at(0) != '.' {
                    ext.push('.');
                }
                ext.push_utfstr(replacement);
            }
        }
        path_out(streams, &opts, ext);

        n_transformed += 1;
    }

    if n_transformed > 0 {
        STATUS_CMD_OK
    } else {
        STATUS_CMD_ERROR
    }
}

fn path_resolve(
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
) -> Option<c_int> {
    let mut opts = Options::default();
    let mut optind = 0;
    let retval = parse_opts(&mut opts, &mut optind, 0, args, parser, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }

    let mut n_transformed = 0usize;
    let arguments = arguments(args, &mut optind, streams).with_split_behavior(match opts.null_in {
        true => SplitBehavior::Null,
        false => SplitBehavior::InferNull,
    });
    for (arg, _) in arguments {
        let mut real = match wrealpath(&arg) {
            Some(p) => p,
            None => {
                // The path doesn't exist, isn't readable or a symlink loop.
                // We go up until we find something that works.
                let mut next = arg;
                // First add $PWD if we're relative
                if !next.is_empty() && next.char_at(0) != '/' {
                    next = path_apply_working_directory(&next, &parser.vars().get_pwd_slash());
                }
                let mut rest = wbasename(&next).to_owned();
                let mut real = None;
                while !next.is_empty() && next != "/" {
                    next = wdirname(&next).to_owned();
                    real = wrealpath(&next);
                    if let Some(ref mut real) = real {
                        real.push('/');
                        real.push_utfstr(&rest);
                        *real = normalize_path(real, false);
                        break;
                    }
                    rest = (wbasename(&next).to_owned() + L!("/")) + rest.as_utfstr();
                }

                match real {
                    Some(p) => p,
                    None => continue,
                }
            }
        };

        // Normalize the path so "../" components are eliminated even after
        // nonexistent or non-directory components.
        // Otherwise `path resolve foo/../` will be `$PWD/foo/../` if foo is a file.
        real = normalize_path(&real, false);

        // Return 0 if we found a realpath.
        if opts.quiet {
            return STATUS_CMD_OK;
        }
        path_out(streams, &opts, real);
        n_transformed += 1;
    }

    if n_transformed > 0 {
        STATUS_CMD_OK
    } else {
        STATUS_CMD_ERROR
    }
}

fn path_sort(parser: &Parser, streams: &mut IoStreams<'_>, args: &mut [WString]) -> Option<c_int> {
    let mut opts = Options::default();
    opts.reverse_valid = true;
    opts.unique_valid = true;
    let mut optind = 0;
    let retval = parse_opts(&mut opts, &mut optind, 0, args, parser, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }

    let keyfunc: &dyn Fn(&wstr) -> &wstr = match &opts.key {
        Some(k) if k == "basename" => &wbasename as _,
        Some(k) if k == "dirname" => &wdirname as _,
        Some(k) if k == "path" => {
            // Act as if --key hadn't been given.
            opts.key = None;
            &wbasename as _
        }
        None => &wbasename as _,
        Some(k) => {
            path_error!(streams, "%ls: Invalid sort key '%ls'\n", args[0], k);
            return STATUS_INVALID_ARGS;
        }
    };

    let arguments = arguments(args, &mut optind, streams).with_split_behavior(match opts.null_in {
        true => SplitBehavior::Null,
        false => SplitBehavior::InferNull,
    });

    let mut list: Vec<_> = arguments.map(|(f, _)| f).collect();

    if opts.key.is_some() {
        // We use a stable sort here
        list.sort_by(|a, b| {
            match wcsfilecmp_glob(keyfunc(a), keyfunc(b)) {
                // to avoid changing the order so we can chain calls
                Ordering::Equal => Ordering::Greater,
                order if opts.reverse => order.reverse(),
                order => order,
            }
        });

        if opts.unique {
            // we are sorted, dedup will remove all duplicates
            list.dedup_by(|a, b| keyfunc(a) == keyfunc(b));
        }
    } else {
        // Without --key, we just sort by the entire path,
        // so we have no need to transform and such.
        list.sort_by(|a, b| {
            match wcsfilecmp_glob(a, b) {
                // to avoid changing the order so we can chain calls
                Ordering::Equal => Ordering::Greater,
                order if opts.reverse => order.reverse(),
                order => order,
            }
        });

        if opts.unique {
            // we are sorted, dedup will remove all duplicates
            list.dedup();
        }
    }

    for entry in list {
        path_out(streams, &opts, &entry);
    }

    /* TODO: Return true only if already sorted? */
    STATUS_CMD_OK
}

fn filter_path(opts: &Options, path: &wstr) -> bool {
    // TODO: Add moar stuff:
    // fifos, sockets, size greater than zero, setuid, ...
    // Nothing to check, file existence is checked elsewhere.
    if opts.types.is_none() && opts.perms.is_none() {
        return true;
    }

    if let Some(t) = opts.types {
        let mut type_ok = false;
        if t.contains(TypeFlags::LINK) {
            let md = lwstat(path);
            type_ok = md.is_ok() && md.unwrap().is_symlink();
        }
        let Ok(md) = wstat(path) else {
            // Does not exist
            return false;
        };

        let ft = md.file_type();
        type_ok = match type_ok {
            true => true,
            _ if t.contains(TypeFlags::FILE) && ft.is_file() => true,
            _ if t.contains(TypeFlags::DIR) && ft.is_dir() => true,
            _ if t.contains(TypeFlags::BLOCK) && ft.is_block_device() => true,
            _ if t.contains(TypeFlags::CHAR) && ft.is_char_device() => true,
            _ if t.contains(TypeFlags::FIFO) && ft.is_fifo() => true,
            _ if t.contains(TypeFlags::SOCK) && ft.is_socket() => true,
            _ => false,
        };

        if !type_ok {
            return false;
        }
    }

    if let Some(perm) = opts.perms {
        let mut amode = 0;
        // TODO: Update bitflags so this works
        /*
        for f in perm {
            amode |= match f {
                PermFlags::READ => R_OK,
                PermFlags::WRITE => W_OK,
                PermFlags::EXEC => X_OK,
                _ => PermFlags::empty(),
            }
        }
        */
        if perm.contains(PermFlags::READ) {
            amode |= R_OK;
        }
        if perm.contains(PermFlags::WRITE) {
            amode |= W_OK;
        }
        if perm.contains(PermFlags::EXEC) {
            amode |= X_OK;
        }
        // access returns 0 on success,
        // -1 on failure. Yes, C can't even keep its bools straight.
        if waccess(path, amode) != 0 {
            return false;
        }

        // Permissions that require special handling
        if perm.is_special() {
            let Ok(md) = wstat(path) else {
                // Does not exist, even though we just checked we can access it
                // likely some kind of race condition
                // We might want to warn the user about this?
                return false;
            };

            #[allow(clippy::if_same_then_else)]
            if perm.contains(PermFlags::SUID) && (md.mode() as mode_t & S_ISUID) == 0 {
                return false;
            } else if perm.contains(PermFlags::SGID) && (md.mode() as mode_t & S_ISGID) == 0 {
                return false;
            } else if perm.contains(PermFlags::USER) && (unsafe { geteuid() } != md.uid() as uid_t)
            {
                return false;
            } else if perm.contains(PermFlags::GROUP) && (unsafe { getegid() } != md.gid() as uid_t)
            {
                return false;
            }
        }
    }

    // No filters failed.
    true
}

fn path_filter_maybe_is(
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
    is_is: bool,
) -> Option<c_int> {
    let mut opts = Options::default();
    opts.types_valid = true;
    opts.perms_valid = true;
    opts.invert_valid = true;
    let mut optind = 0;
    let retval = parse_opts(&mut opts, &mut optind, 0, args, parser, streams);
    if retval != STATUS_CMD_OK {
        return retval;
    }

    // If we have been invoked as "path is", which is "path filter -q".
    if is_is {
        opts.quiet = true;
    }

    let mut n_transformed = 0;
    let arguments = arguments(args, &mut optind, streams).with_split_behavior(match opts.null_in {
        true => SplitBehavior::Null,
        false => SplitBehavior::InferNull,
    });
    for (arg, _) in arguments.filter(|(f, _)| {
        (opts.perms.is_none() && opts.types.is_none()) || (filter_path(&opts, f) != opts.invert)
    }) {
        // If we don't have filters, check if it exists.
        if opts.perms.is_none() && opts.types.is_none() {
            let ok = waccess(&arg, F_OK) == 0;
            if ok == opts.invert {
                continue;
            }
        }

        // We *know* this is a filename,
        // and so if it starts with a `-` we *know* it is relative
        // to $PWD. So we can add `./`.
        // Empty paths make no sense, but e.g. wbasename returns true for them.
        if !arg.is_empty() && arg.starts_with('-') {
            let out = WString::from("./") + &arg[..];
            path_out(streams, &opts, out);
        } else {
            path_out(streams, &opts, arg);
        }
        n_transformed += 1;
        if opts.quiet {
            return STATUS_CMD_OK;
        };
    }

    if n_transformed > 0 {
        STATUS_CMD_OK
    } else {
        STATUS_CMD_ERROR
    }
}

fn path_filter(
    parser: &Parser,
    streams: &mut IoStreams<'_>,
    args: &mut [WString],
) -> Option<c_int> {
    path_filter_maybe_is(parser, streams, args, false)
}

fn path_is(parser: &Parser, streams: &mut IoStreams<'_>, args: &mut [WString]) -> Option<c_int> {
    path_filter_maybe_is(parser, streams, args, true)
}

/// The path builtin, for handling paths.
pub fn path(parser: &Parser, streams: &mut IoStreams<'_>, args: &mut [WString]) -> Option<c_int> {
    let cmd = &args[0];
    let argc = args.len();

    if argc <= 1 {
        streams
            .err
            .append(&wgettext_fmt!(BUILTIN_ERR_MISSING_SUBCMD, cmd));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return STATUS_INVALID_ARGS;
    }

    if args[1] == "-h" || args[1] == "--help" {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    let subcmd_name = &args[1];

    let subcmd: BuiltinCmd = match subcmd_name.to_string().as_str() {
        "basename" => path_basename,
        "change-extension" => path_change_extension,
        "dirname" => path_dirname,
        "extension" => path_extension,
        "filter" => path_filter,
        "is" => path_is,
        "mtime" => path_mtime,
        "normalize" => path_normalize,
        "resolve" => path_resolve,
        "sort" => path_sort,
        _ => {
            streams
                .err
                .append(&wgettext_fmt!(BUILTIN_ERR_INVALID_SUBCMD, cmd, subcmd_name));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return STATUS_INVALID_ARGS;
        }
    };

    if argc >= 3 && (args[2] == "-h" || args[2] == "--help") {
        // Unlike string, we don't have separate docs (yet)
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }
    let args = &mut args[1..];
    return subcmd(parser, streams, args);
}

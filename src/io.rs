use crate::builtins::shared::{STATUS_CMD_ERROR, STATUS_CMD_OK, STATUS_READ_TOO_MUCH};
use crate::common::{str2wcstring, wcs2string, EMPTY_STRING};
use crate::fd_monitor::{
    Callback, FdMonitor, FdMonitorItem, FdMonitorItemId, ItemAction, ItemWakeReason,
};
use crate::fds::{
    make_autoclose_pipes, make_fd_nonblocking, wopen_cloexec, AutoCloseFd, PIPE_ERROR,
};
use crate::flog::{should_flog, FLOG, FLOGF};
use crate::global_safety::RelaxedAtomicBool;
use crate::nix::isatty;
use crate::path::path_apply_working_directory;
use crate::proc::JobGroupRef;
use crate::redirection::{RedirectionMode, RedirectionSpecList};
use crate::signal::SigChecker;
use crate::topic_monitor::Topic;
use crate::wchar::prelude::*;
use crate::wutil::{perror, perror_io, wdirname, wstat, wwrite_to_fd};
use errno::Errno;
use libc::{EAGAIN, EINTR, ENOENT, ENOTDIR, EPIPE, EWOULDBLOCK, STDOUT_FILENO};
use nix::fcntl::OFlag;
use nix::sys::stat::Mode;
use std::cell::{RefCell, UnsafeCell};
use std::fs::File;
use std::io;
use std::os::fd::{AsRawFd, IntoRawFd, OwnedFd, RawFd};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Condvar, Mutex, MutexGuard};

/// separated_buffer_t represents a buffer of output from commands, prepared to be turned into a
/// variable. For example, command substitutions output into one of these. Most commands just
/// produce a stream of bytes, and those get stored directly. However other commands produce
/// explicitly separated output, in particular `string` like `string collect` and `string split0`.
/// The buffer tracks a sequence of elements. Some elements are explicitly separated and should not
/// be further split; other elements have inferred separation and may be split by IFS (or not,
/// depending on its value).
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum SeparationType {
    /// this element should be further separated by IFS
    inferred,
    /// this element is explicitly separated and should not be further split
    explicitly,
}

pub struct BufferElement {
    pub contents: Vec<u8>,
    pub separation: SeparationType,
}

impl BufferElement {
    pub fn new(contents: Vec<u8>, separation: SeparationType) -> Self {
        BufferElement {
            contents,
            separation,
        }
    }
    pub fn is_explicitly_separated(&self) -> bool {
        self.separation == SeparationType::explicitly
    }
}

/// A separated_buffer_t contains a list of elements, some of which may be separated explicitly and
/// others which must be separated further by the user (e.g. via IFS).
pub struct SeparatedBuffer {
    /// Limit on how much data we'll buffer. Zero means no limit.
    buffer_limit: usize,
    /// Current size of all contents.
    contents_size: usize,
    /// List of buffer elements.
    elements: Vec<BufferElement>,
    /// True if we're discarding input because our buffer_limit has been exceeded.
    discard: bool,
}

impl SeparatedBuffer {
    pub fn new(limit: usize) -> Self {
        SeparatedBuffer {
            buffer_limit: limit,
            contents_size: 0,
            elements: vec![],
            discard: false,
        }
    }

    /// Return the buffer limit size, or 0 for no limit.
    pub fn limit(&self) -> usize {
        self.buffer_limit
    }

    /// Return the contents size.
    pub fn len(&self) -> usize {
        self.contents_size
    }

    /// Return whether the output has been discarded.
    pub fn discarded(&self) -> bool {
        self.discard
    }

    /// Serialize the contents to a single string, where explicitly separated elements have a
    /// newline appended.
    pub fn newline_serialized(&self) -> Vec<u8> {
        let mut result = Vec::with_capacity(self.len());
        for elem in &self.elements {
            result.extend_from_slice(&elem.contents);
            if elem.is_explicitly_separated() {
                result.push(b'\n');
            }
        }
        result
    }

    /// Return the list of elements.
    pub fn elements(&self) -> &[BufferElement] {
        &self.elements
    }

    /// Append the given data with separation type `sep`.
    pub fn append(&mut self, data: &[u8], sep: SeparationType) -> bool {
        if !self.try_add_size(data.len()) {
            return false;
        }
        // Try merging with the last element.
        if sep == SeparationType::inferred && self.last_inferred() {
            self.elements
                .last_mut()
                .unwrap()
                .contents
                .extend_from_slice(data);
        } else {
            self.elements.push(BufferElement::new(data.to_vec(), sep));
        }
        true
    }

    /// Remove all elements and unset the discard flag.
    pub fn clear(&mut self) {
        self.elements.clear();
        self.contents_size = 0;
        self.discard = false;
    }

    /// Return true if our last element has an inferred separation type.
    fn last_inferred(&self) -> bool {
        !self.elements.is_empty() && !self.elements.last().unwrap().is_explicitly_separated()
    }

    /// Mark that we are about to add the given size `delta` to the buffer. Return true if we
    /// succeed, false if we exceed buffer_limit.
    fn try_add_size(&mut self, delta: usize) -> bool {
        if self.discard {
            return false;
        }
        let proposed_size = self.contents_size + delta;
        if proposed_size < delta || (self.buffer_limit > 0 && proposed_size > self.buffer_limit) {
            self.clear();
            self.discard = true;
            return false;
        }
        self.contents_size = proposed_size;
        true
    }
}

/// Describes what type of IO operation an io_data_t represents.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum IoMode {
    file,
    pipe,
    fd,
    close,
    bufferfill,
}

/// Represents a FD redirection.
pub trait IoData {
    /// Type of redirect.
    fn io_mode(&self) -> IoMode;
    /// FD to redirect.
    fn fd(&self) -> RawFd;
    /// Source fd. This is dup2'd to fd, or if it is -1, then fd is closed.
    /// That is, we call dup2(source_fd, fd).
    fn source_fd(&self) -> RawFd;
    fn print(&self);
    // The address of the object, for comparison.
    fn as_ptr(&self) -> *const ();
    fn as_bufferfill(&self) -> Option<&IoBufferfill> {
        None
    }
}

// todo!("this should be safe because of how it's used. Rationalize this better.")
pub trait IoDataSync: IoData + Send + Sync {}
unsafe impl Send for IoClose {}
unsafe impl Send for IoFd {}
unsafe impl Send for IoFile {}
unsafe impl Send for IoPipe {}
unsafe impl Send for IoBufferfill {}
unsafe impl Sync for IoClose {}
unsafe impl Sync for IoFd {}
unsafe impl Sync for IoFile {}
unsafe impl Sync for IoPipe {}
unsafe impl Sync for IoBufferfill {}
impl IoDataSync for IoClose {}
impl IoDataSync for IoFd {}
impl IoDataSync for IoFile {}
impl IoDataSync for IoPipe {}
impl IoDataSync for IoBufferfill {}

pub struct IoClose {
    fd: RawFd,
}
impl IoClose {
    pub fn new(fd: RawFd) -> Self {
        IoClose { fd }
    }
}
impl IoData for IoClose {
    fn io_mode(&self) -> IoMode {
        IoMode::close
    }
    fn fd(&self) -> RawFd {
        self.fd
    }
    fn source_fd(&self) -> RawFd {
        -1
    }
    fn print(&self) {
        eprintf!("close %d\n", self.fd)
    }
    fn as_ptr(&self) -> *const () {
        (self as *const Self).cast()
    }
}

pub struct IoFd {
    fd: RawFd,
    source_fd: RawFd,
}
impl IoFd {
    /// fd to redirect specified fd to. For example, in 2>&1, source_fd is 1, and io_data_t::fd
    /// is 2.
    pub fn new(fd: RawFd, source_fd: RawFd) -> Self {
        IoFd { fd, source_fd }
    }
}
impl IoData for IoFd {
    fn io_mode(&self) -> IoMode {
        IoMode::fd
    }
    fn fd(&self) -> RawFd {
        self.fd
    }
    fn source_fd(&self) -> RawFd {
        self.source_fd
    }
    fn print(&self) {
        eprintf!("FD map %d -> %d\n", self.source_fd, self.fd)
    }
    fn as_ptr(&self) -> *const () {
        (self as *const Self).cast()
    }
}

/// Represents a redirection to or from an opened file.
pub struct IoFile {
    fd: RawFd,
    // The file which we are writing to or reading from.
    file: File,
}
impl IoFile {
    pub fn new(fd: RawFd, file: File) -> Self {
        IoFile { fd, file }
        // Invalid file redirections are replaced with a closed fd, so the following
        // assertion isn't guaranteed to pass:
        // assert(file_fd_.valid() && "File is not valid");
    }
}
impl IoData for IoFile {
    fn io_mode(&self) -> IoMode {
        IoMode::file
    }
    fn fd(&self) -> RawFd {
        self.fd
    }
    fn source_fd(&self) -> RawFd {
        self.file.as_raw_fd()
    }
    fn print(&self) {
        eprintf!("file %d -> %d\n", self.file.as_raw_fd(), self.fd)
    }
    fn as_ptr(&self) -> *const () {
        (self as *const Self).cast()
    }
}

/// Represents (one end) of a pipe.
pub struct IoPipe {
    fd: RawFd,
    // The pipe's fd. Conceptually this is dup2'd to io_data_t::fd.
    pipe_fd: OwnedFd,
    /// Whether this is an input pipe. This is used only for informational purposes.
    is_input: bool,
}
impl IoPipe {
    pub fn new(fd: RawFd, is_input: bool, pipe_fd: OwnedFd) -> Self {
        IoPipe {
            fd,
            pipe_fd,
            is_input,
        }
    }
}
impl IoData for IoPipe {
    fn io_mode(&self) -> IoMode {
        IoMode::pipe
    }
    fn fd(&self) -> RawFd {
        self.fd
    }
    fn source_fd(&self) -> RawFd {
        self.pipe_fd.as_raw_fd()
    }
    fn print(&self) {
        eprintf!(
            "pipe {%d} (input: %s) -> %d\n",
            self.source_fd(),
            if self.is_input { "yes" } else { "no" },
            self.fd
        )
    }
    fn as_ptr(&self) -> *const () {
        (self as *const Self).cast()
    }
}

/// Represents filling an io_buffer_t. Very similar to io_pipe_t.
pub struct IoBufferfill {
    target: RawFd,

    /// Write end. The other end is connected to an io_buffer_t.
    write_fd: OwnedFd,

    /// The receiving buffer.
    buffer: Arc<IoBuffer>,
}
impl IoBufferfill {
    /// Create an io_bufferfill_t which, when written from, fills a buffer with the contents.
    /// Returns an error on failure, e.g. too many open fds.
    pub fn create() -> io::Result<Arc<IoBufferfill>> {
        Self::create_opts(0, STDOUT_FILENO)
    }
    /// Create an io_bufferfill_t which, when written from, fills a buffer with the contents.
    /// Returns an error on failure, e.g. too many open fds.
    ///
    /// \param target the fd which this will be dup2'd to - typically stdout.
    pub fn create_opts(buffer_limit: usize, target: RawFd) -> io::Result<Arc<IoBufferfill>> {
        assert!(target >= 0, "Invalid target fd");

        // Construct our pipes.
        let pipes = make_autoclose_pipes()?;
        // Our buffer will read from the read end of the pipe. This end must be non-blocking. This is
        // because our fillthread needs to poll to decide if it should shut down, and also accept input
        // from direct buffer transfers.
        match make_fd_nonblocking(pipes.read.as_raw_fd()) {
            Ok(_) => (),
            Err(e) => {
                FLOG!(warning, PIPE_ERROR);
                perror_io("fcntl", &e);
                return Err(e);
            }
        }
        // Our fillthread gets the read end of the pipe; out_pipe gets the write end.
        let buffer = Arc::new(IoBuffer::new(buffer_limit));
        begin_filling(&buffer, pipes.read);
        Ok(Arc::new(IoBufferfill {
            target,
            write_fd: pipes.write,
            buffer,
        }))
    }

    pub fn buffer_ref(&self) -> &Arc<IoBuffer> {
        &self.buffer
    }

    pub fn buffer(&self) -> &IoBuffer {
        &self.buffer
    }

    /// Reset the receiver (possibly closing the write end of the pipe), and complete the fillthread
    /// of the buffer. Return the buffer.
    pub fn finish(filler: Arc<IoBufferfill>) -> SeparatedBuffer {
        // The io filler is passed in. This typically holds the only instance of the write side of the
        // pipe used by the buffer's fillthread (except for that side held by other processes). Get the
        // buffer out of the bufferfill and clear the shared_ptr; this will typically widow the pipe.
        // Then allow the buffer to finish.
        filler
            .buffer
            .complete_background_fillthread_and_take_buffer()
    }
}
impl IoData for IoBufferfill {
    fn io_mode(&self) -> IoMode {
        IoMode::bufferfill
    }
    fn fd(&self) -> RawFd {
        self.target
    }
    fn source_fd(&self) -> RawFd {
        self.write_fd.as_raw_fd()
    }
    fn print(&self) {
        eprintf!(
            "bufferfill %d -> %d\n",
            self.write_fd.as_raw_fd(),
            self.fd()
        )
    }
    fn as_ptr(&self) -> *const () {
        (self as *const Self).cast()
    }
    fn as_bufferfill(&self) -> Option<&IoBufferfill> {
        Some(self)
    }
}

/// An io_buffer_t is a buffer which can populate itself by reading from an fd.
/// It is not an io_data_t.
pub struct IoBuffer {
    /// Buffer storing what we have read.
    buffer: Mutex<SeparatedBuffer>,

    /// Atomic flag indicating our fillthread should shut down.
    shutdown_fillthread: RelaxedAtomicBool,

    /// A promise, allowing synchronization with the background fill operation.
    /// The operation has a reference to this as well, and fulfills this promise when it exits.
    #[allow(clippy::type_complexity)]
    fill_waiter: RefCell<Option<Arc<(Mutex<bool>, Condvar)>>>,

    /// The item id of our background fillthread fd monitor item.
    item_id: AtomicU64,
}

// safety: todo!("rationalize why fill_waiter is safe")
unsafe impl Send for IoBuffer {}
unsafe impl Sync for IoBuffer {}

impl IoBuffer {
    pub fn new(limit: usize) -> Self {
        IoBuffer {
            buffer: Mutex::new(SeparatedBuffer::new(limit)),
            shutdown_fillthread: RelaxedAtomicBool::new(false),
            fill_waiter: RefCell::new(None),
            item_id: AtomicU64::new(0),
        }
    }

    /// Append a string to the buffer.
    pub fn append(&self, data: &[u8], typ: SeparationType) -> bool {
        self.buffer.lock().unwrap().append(data, typ)
    }

    /// Return true if output was discarded due to exceeding the read limit.
    pub fn discarded(&self) -> bool {
        self.buffer.lock().unwrap().discarded()
    }

    /// Read some, filling the buffer. The buffer is passed in to enforce that the append lock is
    /// held. Return positive on success, 0 if closed, -1 on error (in which case errno will be
    /// set).
    pub fn read_once(fd: RawFd, buffer: &mut MutexGuard<'_, SeparatedBuffer>) -> isize {
        assert!(fd >= 0, "Invalid fd");
        errno::set_errno(Errno(0));
        let mut bytes = [b'\0'; 4096 * 4];

        // We want to swallow EINTR only; in particular EAGAIN needs to be returned back to the caller.
        let amt = loop {
            let amt = unsafe {
                libc::read(
                    fd,
                    std::ptr::addr_of_mut!(bytes).cast(),
                    std::mem::size_of_val(&bytes),
                )
            };
            if amt < 0 && errno::errno().0 == EINTR {
                continue;
            }
            break amt;
        };
        if amt < 0 && ![EAGAIN, EWOULDBLOCK].contains(&errno::errno().0) {
            perror("read");
        } else if amt > 0 {
            buffer.append(
                &bytes[0..usize::try_from(amt).unwrap()],
                SeparationType::inferred,
            );
        }
        amt
    }

    /// End the background fillthread operation, and return the buffer, transferring ownership.
    pub fn complete_background_fillthread_and_take_buffer(&self) -> SeparatedBuffer {
        // Mark that our fillthread is done, then wake it up.
        assert!(self.fillthread_running(), "Should have a fillthread");
        assert!(
            self.item_id.load(Ordering::SeqCst) != 0,
            "Should have a valid item ID"
        );
        self.shutdown_fillthread.store(true);
        fd_monitor().poke_item(FdMonitorItemId::from(self.item_id.load(Ordering::SeqCst)));

        // Wait for the fillthread to fulfill its promise, and then clear the future so we know we no
        // longer have one.

        let mut promise = self.fill_waiter.borrow_mut();
        let (mutex, condvar) = &**promise.as_ref().unwrap();
        {
            let done_guard = mutex.lock().unwrap();
            let _done_guard = condvar.wait_while(done_guard, |done| !*done).unwrap();
        }
        *promise = None;

        // Return our buffer, transferring ownership.
        let mut locked_buff = self.buffer.lock().unwrap();
        let mut result = SeparatedBuffer::new(locked_buff.limit());
        std::mem::swap(&mut result, &mut locked_buff);
        locked_buff.clear();
        result
    }

    /// Helper to return whether the fillthread is running.
    pub fn fillthread_running(&self) -> bool {
        return self.fill_waiter.borrow().is_some();
    }
}

/// Begin the fill operation, reading from the given fd in the background.
fn begin_filling(iobuffer: &Arc<IoBuffer>, fd: OwnedFd) {
    assert!(!iobuffer.fillthread_running(), "Already have a fillthread");

    // We want to fill buffer_ by reading from fd. fd is the read end of a pipe; the write end is
    // owned by another process, or something else writing in fish.
    // Pass fd to an fd_monitor. It will add fd to its select() loop, and give us a callback when
    // the fd is readable, or when our item is poked. The usual path is that we will get called
    // back, read a bit from the fd, and append it to the buffer. Eventually the write end of the
    // pipe will be closed - probably the other process exited - and fd will be widowed; read() will
    // then return 0 and we will stop reading.
    // In exotic circumstances the write end of the pipe will not be closed; this may happen in
    // e.g.:
    //   cmd ( background & ; echo hi )
    // Here the background process will inherit the write end of the pipe and hold onto it forever.
    // In this case, when complete_background_fillthread() is called, the callback will be invoked
    // with item_wake_reason_t::poke, and we will notice that the shutdown flag is set (this
    // indicates that the command substitution is done); in this case we will read until we get
    // EAGAIN and then give up.

    // Construct a promise. We will fulfill it in our fill thread, and wait for it in
    // complete_background_fillthread(). Note that TSan complains if the promise's dtor races with
    // the future's call to wait(), so we store the promise, not just its future (#7681).
    let promise = Arc::new((Mutex::new(false), Condvar::new()));
    iobuffer.fill_waiter.replace(Some(promise.clone()));
    // Run our function to read until the receiver is closed.
    // It's OK to capture 'buffer' because 'this' waits for the promise in its dtor.
    let item_callback: Callback = {
        let iobuffer = iobuffer.clone();
        Box::new(move |fd: &mut AutoCloseFd, reason: ItemWakeReason| {
            // Only check the shutdown flag if we timed out or were poked.
            // It's important that if select() indicated we were readable, that we call select() again
            // allowing it to time out. Note the typical case is that the fd will be closed, in which
            // case select will return immediately.
            let mut done = false;
            if reason == ItemWakeReason::Readable {
                // select() reported us as readable; read a bit.
                let mut buf = iobuffer.buffer.lock().unwrap();
                let ret = IoBuffer::read_once(fd.as_raw_fd(), &mut buf);
                done = ret == 0 || (ret < 0 && ![EAGAIN, EWOULDBLOCK].contains(&errno::errno().0));
            } else if iobuffer.shutdown_fillthread.load() {
                // Here our caller asked us to shut down; read while we keep getting data.
                // This will stop when the fd is closed or if we get EAGAIN.
                let mut buf = iobuffer.buffer.lock().unwrap();
                loop {
                    let ret = IoBuffer::read_once(fd.as_raw_fd(), &mut buf);
                    if ret <= 0 {
                        break;
                    }
                }
                done = true;
            }
            if !done {
                ItemAction::Retain
            } else {
                fd.close();
                let (mutex, condvar) = &*promise;
                {
                    let mut done = mutex.lock().unwrap();
                    *done = true;
                }
                condvar.notify_one();
                ItemAction::Remove
            }
        })
    };

    let fd = AutoCloseFd::new(fd.into_raw_fd());
    let item_id = fd_monitor().add(FdMonitorItem::new(fd, None, item_callback));
    iobuffer.item_id.store(u64::from(item_id), Ordering::SeqCst);
}

pub type IoDataRef = Arc<dyn IoDataSync>;

#[derive(Clone, Default)]
pub struct IoChain(pub Vec<IoDataRef>);

impl IoChain {
    pub fn new() -> Self {
        Default::default()
    }
    pub fn remove(&mut self, element: &dyn IoDataSync) {
        let element = element as *const _;
        let element = element as *const ();
        self.0.retain(|e| {
            let e = Arc::as_ptr(e) as *const ();
            !std::ptr::eq(e, element)
        });
    }
    pub fn clear(&mut self) {
        self.0.clear()
    }
    pub fn push(&mut self, element: IoDataRef) {
        self.0.push(element);
    }
    pub fn append(&mut self, chain: &IoChain) -> bool {
        self.0.extend_from_slice(&chain.0);
        true
    }

    /// Return the last io redirection in the chain for the specified file descriptor, or nullptr
    /// if none.
    pub fn io_for_fd(&self, fd: RawFd) -> Option<IoDataRef> {
        self.0.iter().rev().find(|data| data.fd() == fd).cloned()
    }

    /// Attempt to resolve a list of redirection specs to IOs, appending to 'this'.
    /// Return true on success, false on error, in which case an error will have been printed.
    #[allow(clippy::collapsible_else_if)]
    pub fn append_from_specs(&mut self, specs: &RedirectionSpecList, pwd: &wstr) -> bool {
        let mut have_error = false;

        let print_error = |err, target: &wstr| {
            // If the error is that the file doesn't exist
            // or there's a non-directory component,
            // find the first problematic component for a better message.
            if [ENOENT, ENOTDIR].contains(&err) {
                FLOGF!(warning, FILE_ERROR, target);
                let mut dname: &wstr = target;
                while !dname.is_empty() {
                    let next: &wstr = wdirname(dname);
                    if let Ok(md) = wstat(next) {
                        if !md.is_dir() {
                            FLOGF!(warning, "Path '%ls' is not a directory", next);
                        } else {
                            FLOGF!(warning, "Path '%ls' does not exist", dname);
                        }
                        break;
                    }
                    dname = next;
                }
            } else if err != EINTR {
                // If we get EINTR we had a cancel signal.
                // That's expected (ctrl-c on the commandline),
                // so no warning.
                FLOGF!(warning, FILE_ERROR, target);
                perror("open");
            }
        };

        for spec in specs {
            match spec.mode {
                RedirectionMode::fd => {
                    if spec.is_close() {
                        self.push(Arc::new(IoClose::new(spec.fd)));
                    } else {
                        let target_fd = spec
                            .get_target_as_fd()
                            .expect("fd redirection should have been validated already");
                        self.push(Arc::new(IoFd::new(spec.fd, target_fd)));
                    }
                }
                _ => {
                    // We have a path-based redirection. Resolve it to a file.
                    // Mark it as CLO_EXEC because we don't want it to be open in any child.
                    let path = path_apply_working_directory(&spec.target, pwd);
                    let oflags = spec.oflags();

                    match wopen_cloexec(&path, oflags, OPEN_MASK) {
                        Ok(file) => {
                            self.push(Arc::new(IoFile::new(spec.fd, file)));
                        }
                        Err(err) => {
                            if oflags.contains(OFlag::O_EXCL) && err == nix::Error::EEXIST {
                                FLOGF!(warning, NOCLOB_ERROR, spec.target);
                            } else if spec.mode != RedirectionMode::try_input {
                                if should_flog!(warning) {
                                    print_error(errno::errno().0, &spec.target);
                                }
                            }
                            // If opening a file fails, insert a closed FD instead of the file redirection
                            // and return false. This lets execution potentially recover and at least gives
                            // the shell a chance to gracefully regain control of the shell (see #7038).
                            if spec.mode != RedirectionMode::try_input {
                                self.push(Arc::new(IoClose::new(spec.fd)));
                                have_error = true;
                                continue;
                            } else {
                                // If we're told to try via `<?`, we use /dev/null
                                match wopen_cloexec(L!("/dev/null"), oflags, OPEN_MASK) {
                                    Ok(fd) => {
                                        self.push(Arc::new(IoFile::new(spec.fd, fd)));
                                    }
                                    _ => {
                                        // /dev/null can't be opened???
                                        if should_flog!(warning) {
                                            print_error(errno::errno().0, L!("/dev/null"));
                                        }
                                        self.push(Arc::new(IoClose::new(spec.fd)));
                                        have_error = true;
                                        continue;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        !have_error
    }

    /// Output debugging information to stderr.
    pub fn print(&self) {
        if self.0.is_empty() {
            eprintf!(
                "Empty chain %s\n",
                format!("{:p}", std::ptr::addr_of!(self))
            );
            return;
        }

        eprintf!(
            "Chain %s (%ld items):\n",
            format!("{:p}", std::ptr::addr_of!(self)),
            self.0.len()
        );
        for (i, io) in self.0.iter().enumerate() {
            eprintf!("\t%lu: fd:%d, ", i, io.fd());
            io.print();
        }
    }
}

/// Base class representing the output that a builtin can generate.
/// This has various subclasses depending on the ultimate output destination.
pub enum OutputStream {
    /// A null output stream which ignores all writes.
    Null,
    Fd(FdOutputStream),
    String(StringOutputStream),
    Buffered(BufferedOutputStream),
}

impl OutputStream {
    /// Return any internally buffered contents.
    /// This is only implemented for a string_output_stream; others flush data to their underlying
    /// receiver (fd, or separated buffer) immediately and so will return an empty string here.
    pub fn contents(&self) -> &wstr {
        match self {
            OutputStream::String(stream) => stream.contents(),
            OutputStream::Null | OutputStream::Fd(_) | OutputStream::Buffered(_) => &EMPTY_STRING,
        }
    }

    /// Flush any unwritten data to the underlying device, and return an error code.
    /// A 0 code indicates success. The base implementation returns 0.
    pub fn flush_and_check_error(&mut self) -> libc::c_int {
        match self {
            OutputStream::Fd(stream) => stream.flush_and_check_error(),
            OutputStream::Buffered(stream) => stream.flush_and_check_error(),
            OutputStream::Null | OutputStream::String(_) => STATUS_CMD_OK.unwrap(),
        }
    }

    /// Append a &wstr or WString.
    pub fn append<Str: AsRef<wstr>>(&mut self, s: Str) -> bool {
        let s = &s.as_ref();
        match self {
            OutputStream::Null => true,
            OutputStream::Fd(stream) => stream.append(s),
            OutputStream::String(stream) => stream.append(s),
            OutputStream::Buffered(stream) => stream.append(s),
        }
    }

    /// An optional override point. This is for explicit separation.
    /// \param want_newline this is true if the output item should be ended with a newline. This
    /// is only relevant if we are printing the output to a stream,
    pub fn append_with_separation(
        &mut self,
        s: &wstr,
        typ: SeparationType,
        want_newline: bool,
    ) -> bool {
        match self {
            OutputStream::Buffered(stream) => stream.append_with_separation(s, typ, want_newline),
            OutputStream::Fd(_) | OutputStream::Null | OutputStream::String(_) => {
                if typ == SeparationType::explicitly && want_newline {
                    // Try calling "append" less - it might write() to an fd
                    let mut buf = s.to_owned();
                    buf.push('\n');
                    self.append(buf)
                } else {
                    self.append(s)
                }
            }
        }
    }

    /// Append a &wstr or WString with a newline
    pub fn appendln(&mut self, s: impl Into<WString>) -> bool {
        let s = s.into() + L!("\n");
        self.append(s)
    }

    pub fn append_char(&mut self, c: char) -> bool {
        self.append(wstr::from_char_slice(&[c]))
    }
    pub fn append1(&mut self, c: char) -> bool {
        self.append_char(c)
    }
    pub fn push_back(&mut self, c: char) -> bool {
        self.append_char(c)
    }
    pub fn push(&mut self, c: char) -> bool {
        self.append(wstr::from_char_slice(&[c]))
    }

    // Append data from a narrow buffer, widening it.
    pub fn append_narrow_buffer(&mut self, buffer: &SeparatedBuffer) -> bool {
        for rhs_elem in buffer.elements() {
            if !self.append_with_separation(
                &str2wcstring(&rhs_elem.contents),
                rhs_elem.separation,
                false,
            ) {
                return false;
            }
        }
        true
    }
}

/// An output stream for builtins which outputs to an fd.
/// Note the fd may be something like stdout; there is no ownership implied here.
pub struct FdOutputStream {
    /// The file descriptor to write to.
    fd: RawFd,

    /// Used to check if a SIGINT has been received when EINTR is encountered
    sigcheck: SigChecker,

    /// Whether we have received an error.
    errored: bool,
}
impl FdOutputStream {
    /// Construct from a file descriptor, which must be nonegative.
    pub fn new(fd: RawFd) -> Self {
        assert!(fd >= 0, "Invalid fd");
        FdOutputStream {
            fd,
            sigcheck: SigChecker::new(Topic::sighupint),
            errored: false,
        }
    }

    fn append(&mut self, s: &wstr) -> bool {
        if self.errored {
            return false;
        }
        if wwrite_to_fd(s, self.fd).is_none() {
            // Some of our builtins emit multiple screens worth of data sent to a pager (the primary
            // example being the `history` builtin) and receiving SIGINT should be considered normal and
            // non-exceptional (user request to abort via Ctrl-C), meaning we shouldn't print an error.
            if errno::errno().0 == EINTR && self.sigcheck.check() {
                // We have two options here: we can either return false without setting errored_ to
                // true (*this* write will be silently aborted but the onus is on the caller to check
                // the return value and skip future calls to `append()`) or we can flag the entire
                // output stream as errored, causing us to both return false and skip any future writes.
                // We're currently going with the latter, especially seeing as no callers currently
                // check the result of `append()` (since it was always a void function before).
            } else if errno::errno().0 != EPIPE {
                perror("write");
            }
            self.errored = true;
        }
        !self.errored
    }

    fn flush_and_check_error(&mut self) -> libc::c_int {
        // Return a generic 1 on any write failure.
        if self.errored {
            STATUS_CMD_ERROR
        } else {
            STATUS_CMD_OK
        }
        .unwrap()
    }
}

/// A simple output stream which buffers into a wcstring.
#[derive(Default)]
pub struct StringOutputStream {
    contents: WString,
}
impl StringOutputStream {
    pub fn new() -> Self {
        Default::default()
    }
    fn append(&mut self, s: &wstr) -> bool {
        self.contents.push_utfstr(s);
        true
    }
    /// Return the wcstring containing the output.
    fn contents(&self) -> &wstr {
        &self.contents
    }
}

/// An output stream for builtins which writes into a separated buffer.
pub struct BufferedOutputStream {
    /// The buffer we are filling.
    buffer: Arc<IoBuffer>,
}
impl BufferedOutputStream {
    pub fn new(buffer: Arc<IoBuffer>) -> Self {
        Self { buffer }
    }
    fn append(&mut self, s: &wstr) -> bool {
        self.buffer.append(&wcs2string(s), SeparationType::inferred)
    }
    fn append_with_separation(
        &mut self,
        s: &wstr,
        typ: SeparationType,
        _want_newline: bool,
    ) -> bool {
        self.buffer.append(&wcs2string(s), typ)
    }
    fn flush_and_check_error(&mut self) -> libc::c_int {
        if self.buffer.discarded() {
            return STATUS_READ_TOO_MUCH.unwrap();
        }
        0
    }
}

pub struct IoStreams<'a> {
    // Streams for out and err.
    pub out: &'a mut OutputStream,
    pub err: &'a mut OutputStream,

    // fd representing stdin. This is not closed by the destructor.
    // Note: if stdin is explicitly closed by `<&-` then this is -1!
    pub stdin_fd: RawFd,

    // Whether stdin is "directly redirected," meaning it is the recipient of a pipe (foo | cmd) or
    // direct redirection (cmd < foo.txt). An "indirect redirection" would be e.g.
    //    begin ; cmd ; end < foo.txt
    // If stdin is closed (cmd <&-) this is false.
    pub stdin_is_directly_redirected: bool,

    // Indicates whether stdout and stderr are specifically piped.
    // If this is set, then the is_redirected flags must also be set.
    pub out_is_piped: bool,
    pub err_is_piped: bool,

    // Indicates whether stdout and stderr are at all redirected (e.g. to a file or piped).
    pub out_is_redirected: bool,
    pub err_is_redirected: bool,

    // Actual IO redirections. This is only used by the source builtin.
    pub io_chain: &'a IoChain,

    // The job group of the job, if any. This enables builtins which run more code like eval() to
    // share pgid.
    // FIXME: this is awkwardly placed.
    pub job_group: Option<JobGroupRef>,
}

impl<'a> IoStreams<'a> {
    pub fn new(
        out: &'a mut OutputStream,
        err: &'a mut OutputStream,
        io_chain: &'a IoChain,
    ) -> Self {
        IoStreams {
            out,
            err,
            stdin_fd: -1,
            stdin_is_directly_redirected: false,
            out_is_piped: false,
            err_is_piped: false,
            out_is_redirected: false,
            err_is_redirected: false,
            io_chain,
            job_group: None,
        }
    }
    pub fn out_is_terminal(&self) -> bool {
        !self.out_is_redirected && isatty(STDOUT_FILENO)
    }
}

/// File redirection error message.
const FILE_ERROR: &wstr = L!("An error occurred while redirecting file '%ls'");
const NOCLOB_ERROR: &wstr = L!("The file '%ls' already exists");

/// Base open mode to pass to calls to open.
const OPEN_MASK: Mode = Mode::from_bits_truncate(0o666);

/// Provide the fd monitor used for background fillthread operations.
fn fd_monitor() -> &'static mut FdMonitor {
    // Deliberately leaked to avoid shutdown dtors.
    static mut FDM: *const UnsafeCell<FdMonitor> = std::ptr::null();
    unsafe {
        if FDM.is_null() {
            FDM = Box::into_raw(Box::new(UnsafeCell::new(FdMonitor::new())))
        }
    }
    let ptr: *mut FdMonitor = unsafe { (*FDM).get() };
    unsafe { &mut *ptr }
}

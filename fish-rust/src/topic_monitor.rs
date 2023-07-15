/*! Topic monitoring support.

Topics are conceptually "a thing that can happen." For example,
delivery of a SIGINT, a child process exits, etc. It is possible to post to a topic, which means
that that thing happened.

Associated with each topic is a current generation, which is a 64 bit value. When you query a
topic, you get back a generation. If on the next query the generation has increased, then it
indicates someone posted to the topic.

For example, if you are monitoring a child process, you can query the sigchld topic. If it has
increased since your last query, it is possible that your child process has exited.

Topic postings may be coalesced. That is there may be two posts to a given topic, yet the
generation only increases by 1. The only guarantee is that after a topic post, the current
generation value is larger than any value previously queried.

Tying this all together is the topic_monitor_t. This provides the current topic generations, and
also provides the ability to perform a blocking wait for any topic to change in a particular topic
set. This is the real power of topics: you can wait for a sigchld signal OR a thread exit.
*/

use crate::fd_readable_set::fd_readable_set_t;
use crate::fds::{self, make_fd_nonblocking, AutoClosePipes};
use crate::flog::{FloggableDebug, FLOG};
use crate::wchar::WString;
use crate::wutil::perror;
use nix::errno::Errno;
use nix::unistd;
use std::cell::{Cell, UnsafeCell};
use std::mem;
use std::pin::Pin;
use std::sync::atomic::{AtomicU64, AtomicU8, Ordering};
use std::sync::{Condvar, Mutex, MutexGuard};
use widestring_suffix::widestrs;

#[cxx::bridge]
mod topic_monitor_ffi {}

/// The list of topics which may be observed.
#[repr(u8)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum topic_t {
    sighupint = 0,     // Corresponds to both SIGHUP and SIGINT signals.
    sigchld = 1,       // Corresponds to SIGCHLD signal.
    internal_exit = 2, // Corresponds to an internal process exit.
}

// XXX: Is it correct to use the default or should the default be invalid_generation?
#[derive(Clone, Default, PartialEq, PartialOrd, Eq, Ord)]
pub struct GenerationsList {
    pub sighupint: Cell<u64>,
    pub sigchld: Cell<u64>,
    pub internal_exit: Cell<u64>,
}

/// Simple value type containing the values for a topic.
/// This should be kept in sync with topic_t.
impl GenerationsList {
    /// Update `self` gen counts to match those of `other`.
    pub fn update(&self, other: &Self) {
        self.sighupint.set(other.sighupint.get());
        self.sigchld.set(other.sigchld.get());
        self.internal_exit.set(other.internal_exit.get());
    }
}

pub type generation_t = u64;

impl FloggableDebug for topic_t {}

/// A generation value which indicates the topic is not of interest.
pub const INVALID_GENERATION: generation_t = std::u64::MAX;

pub fn all_topics() -> [topic_t; 3] {
    [topic_t::sighupint, topic_t::sigchld, topic_t::internal_exit]
}

#[widestrs]
impl GenerationsList {
    pub fn new() -> Self {
        Self::default()
    }

    fn describe(&self) -> WString {
        let mut result = WString::new();
        for gen in self.as_array() {
            if !result.is_empty() {
                result.push(',');
            }
            if gen == INVALID_GENERATION {
                result.push_str("-1");
            } else {
                result.push_str(&gen.to_string());
            }
        }
        return result;
    }

    /// Sets the generation for `topic` to `value`.
    pub fn set(&self, topic: topic_t, value: generation_t) {
        match topic {
            topic_t::sighupint => self.sighupint.set(value),
            topic_t::sigchld => self.sigchld.set(value),
            topic_t::internal_exit => self.internal_exit.set(value),
        }
    }

    /// \return the value for a topic.
    pub fn get(&self, topic: topic_t) -> generation_t {
        match topic {
            topic_t::sighupint => self.sighupint.get(),
            topic_t::sigchld => self.sigchld.get(),
            topic_t::internal_exit => self.internal_exit.get(),
        }
    }

    /// \return ourselves as an array.
    pub fn as_array(&self) -> [generation_t; 3] {
        [
            self.sighupint.get(),
            self.sigchld.get(),
            self.internal_exit.get(),
        ]
    }

    /// Set the value of \p topic to the smaller of our value and the value in \p other.
    pub fn set_min_from(&mut self, topic: topic_t, other: &Self) {
        if self.get(topic) > other.get(topic) {
            self.set(topic, other.get(topic));
        }
    }

    /// \return whether a topic is valid.
    pub fn is_valid(&self, topic: topic_t) -> bool {
        self.get(topic) != INVALID_GENERATION
    }

    /// \return whether any topic is valid.
    pub fn any_valid(&self) -> bool {
        let mut valid = false;
        for gen in self.as_array() {
            if gen != INVALID_GENERATION {
                valid = true;
            }
        }
        valid
    }

    /// Generation list containing invalid generations only.
    pub fn invalid() -> GenerationsList {
        GenerationsList {
            sighupint: INVALID_GENERATION.into(),
            sigchld: INVALID_GENERATION.into(),
            internal_exit: INVALID_GENERATION.into(),
        }
    }
}

/// A simple binary semaphore.
/// On systems that do not support unnamed semaphores (macOS in particular) this is built on top of
/// a self-pipe. Note that post() must be async-signal safe.
pub struct binary_semaphore_t {
    // Whether our semaphore was successfully initialized.
    sem_ok_: bool,

    // The semaphore, if initalized.
    // This is Box'd so it has a stable address.
    sem_: Pin<Box<UnsafeCell<libc::sem_t>>>,

    // Pipes used to emulate a semaphore, if not initialized.
    pipes_: AutoClosePipes,
}

impl binary_semaphore_t {
    pub fn new() -> binary_semaphore_t {
        #[allow(unused_mut, unused_assignments)]
        let mut sem_ok_ = false;
        // sem_t does not have an initializer in Rust so we use zeroed().
        #[allow(unused_mut)]
        let mut sem_ = Pin::from(Box::new(UnsafeCell::new(unsafe { mem::zeroed() })));
        let mut pipes_ = AutoClosePipes::default();
        // sem_init always fails with ENOSYS on Mac and has an annoying deprecation warning.
        // On BSD sem_init uses a file descriptor under the hood which doesn't get CLOEXEC (see #7304).
        // So use fast semaphores on Linux only.
        #[cfg(target_os = "linux")]
        {
            let res = unsafe { libc::sem_init(sem_.get(), 0, 0) };
            sem_ok_ = res == 0;
        }
        if !sem_ok_ {
            let pipes = fds::make_autoclose_pipes();
            assert!(pipes.is_some(), "Failed to make pubsub pipes");
            pipes_ = pipes.unwrap();

            // Whoof. Thread Sanitizer swallows signals and replays them at its leisure, at the
            // point where instrumented code makes certain blocking calls. But tsan cannot interrupt
            // a signal call, so if we're blocked in read() (like the topic monitor wants to be!),
            // we'll never receive SIGCHLD and so deadlock. So if tsan is enabled, we mark our fd as
            // non-blocking (so reads will never block) and use select() to poll it.
            if cfg!(feature = "FISH_TSAN_WORKAROUNDS") {
                let _ = make_fd_nonblocking(pipes_.read.fd());
            }
        }
        binary_semaphore_t {
            sem_ok_,
            sem_,
            pipes_,
        }
    }

    /// Release a waiting thread.
    pub fn post(&self) {
        // Beware, we are in a signal handler.
        if self.sem_ok_ {
            let res = unsafe { libc::sem_post(self.sem_.get()) };
            // sem_post is non-interruptible.
            if res < 0 {
                self.die("sem_post");
            }
        } else {
            // Write exactly one byte.
            let success;
            loop {
                let v: u8 = 0;
                let ret = unistd::write(self.pipes_.write.fd(), std::slice::from_ref(&v));
                if ret.err() == Some(Errno::EINTR) {
                    continue;
                }
                success = ret.is_ok();
                break;
            }
            if !success {
                self.die("write");
            }
        }
    }

    /// Wait for a post.
    /// This loops on EINTR.
    pub fn wait(&self) {
        if self.sem_ok_ {
            let mut res;
            loop {
                res = unsafe { libc::sem_wait(self.sem_.get()) };
                if res < 0 && Errno::last() == Errno::EINTR {
                    continue;
                }
                break;
            }
            // Other errors here are very unexpected.
            if res < 0 {
                self.die("sem_wait");
            }
        } else {
            let fd = self.pipes_.read.fd();
            // We must read exactly one byte.
            loop {
                // Under tsan our notifying pipe is non-blocking, so we would busy-loop on the read()
                // call until data is available (that is, fish would use 100% cpu while waiting for
                // processes). This call prevents that.
                if cfg!(feature = "FISH_TSAN_WORKAROUNDS") {
                    let _ = fd_readable_set_t::is_fd_readable(fd, fd_readable_set_t::kNoTimeout);
                }
                let mut ignored: u8 = 0;
                let amt = unistd::read(fd, std::slice::from_mut(&mut ignored));
                if amt.ok() == Some(1) {
                    break;
                }
                // EAGAIN should only be returned in TSan case.
                if amt.is_err()
                    && (amt.err() != Some(Errno::EINTR) && amt.err() != Some(Errno::EAGAIN))
                {
                    self.die("read");
                }
            }
        }
    }

    pub fn die(&self, msg: &str) {
        perror(msg);
        panic!("die");
    }
}

impl Drop for binary_semaphore_t {
    fn drop(&mut self) {
        // We never use sem_t on Mac. The #ifdef avoids deprecation warnings.
        #[cfg(target_os = "linux")]
        {
            if self.sem_ok_ {
                _ = unsafe { libc::sem_destroy(self.sem_.get()) };
            }
        }
    }
}

impl Default for binary_semaphore_t {
    fn default() -> Self {
        Self::new()
    }
}

/// The topic monitor class. This permits querying the current generation values for topics,
/// optionally blocking until they increase.
/// What we would like to write is that we have a set of topics, and threads wait for changes on a
/// condition variable which is tickled in post(). But this can't work because post() may be called
/// from a signal handler and condition variables are not async-signal safe.
/// So instead the signal handler announces changes via a binary semaphore.
/// In the wait case, what generally happens is:
///   A thread fetches the generations, see they have not changed, and then decides to try to wait.
///   It does so by atomically swapping in STATUS_NEEDS_WAKEUP to the status bits.
///   If that succeeds, it waits on the binary semaphore. The post() call will then wake the thread
///   up. If if failed, then either a post() call updated the status values (so perhaps there is a
///   new topic post) or some other thread won the race and called wait() on the semaphore. Here our
///   thread will wait on the data_notifier_ queue.
type topic_bitmask_t = u8;

fn topic_to_bit(t: topic_t) -> topic_bitmask_t {
    1 << (t as u8)
}

// Some stuff that needs to be protected by the same lock.
#[derive(Default)]
struct data_t {
    /// The current values.
    current: GenerationsList,

    /// A flag indicating that there is a current reader.
    /// The 'reader' is responsible for calling sema_.wait().
    has_reader: bool,
}

/// Sentinel status value indicating that a thread is waiting and needs a wakeup.
/// Note it is an error for this bit to be set and also any topic bit.
const STATUS_NEEDS_WAKEUP: u8 = 128;
type status_bits_t = u8;

#[derive(Default)]
pub struct topic_monitor_t {
    data_: Mutex<data_t>,

    /// Condition variable for broadcasting notifications.
    /// This is associated with data_'s mutex.
    data_notifier_: Condvar,

    /// A status value which describes our current state, managed via atomics.
    /// Three possibilities:
    ///    0:   no changed topics, no thread is waiting.
    ///    128: no changed topics, some thread is waiting and needs wakeup.
    ///    anything else: some changed topic, no thread is waiting.
    ///  Note that if the msb is set (status == 128) no other bit may be set.
    status_: AtomicU8,

    /// Binary semaphore used to communicate changes.
    /// If status_ is STATUS_NEEDS_WAKEUP, then a thread has commited to call wait() on our sema and
    /// this must be balanced by the next call to post(). Note only one thread may wait at a time.
    sema_: binary_semaphore_t,
}

/// The principal topic monitor.
/// Do not attempt to move this into a lazy_static, it must be accessed from a signal handler.
static mut s_principal: *const topic_monitor_t = std::ptr::null();

/// Create a new topic monitor. Exposed for the FFI.
pub fn new_topic_monitor() -> Box<topic_monitor_t> {
    Box::default()
}

impl topic_monitor_t {
    /// Initialize the principal monitor, and return it.
    /// This should be called only on the main thread.
    pub fn initialize() -> &'static Self {
        unsafe {
            if s_principal.is_null() {
                // We simply leak.
                s_principal = Box::into_raw(new_topic_monitor());
            }
            &*s_principal
        }
    }

    pub fn post(&self, topic: topic_t) {
        // Beware, we may be in a signal handler!
        // Atomically update the pending topics.
        let topicbit = topic_to_bit(topic);
        const relaxed: Ordering = Ordering::Relaxed;

        // CAS in our bit, capturing the old status value.
        let mut oldstatus: status_bits_t = 0;
        let mut cas_success = false;
        while !cas_success {
            oldstatus = self.status_.load(relaxed);
            // Clear wakeup bit and set our topic bit.
            let mut newstatus = oldstatus;
            newstatus &= !STATUS_NEEDS_WAKEUP; // note: bitwise not
            newstatus |= topicbit;
            cas_success = self
                .status_
                .compare_exchange_weak(oldstatus, newstatus, relaxed, relaxed)
                .is_ok();
        }
        // Note that if the STATUS_NEEDS_WAKEUP bit is set, no other bits must be set.
        assert!(
            ((oldstatus == STATUS_NEEDS_WAKEUP) == ((oldstatus & STATUS_NEEDS_WAKEUP) != 0)),
            "If STATUS_NEEDS_WAKEUP is set no other bits should be set"
        );

        // If the bit was already set, then someone else posted to this topic and nobody has reacted to
        // it yet. In that case we're done.
        if (oldstatus & topicbit) != 0 {
            return;
        }

        // We set a new bit.
        // Check if we should wake up a thread because it was waiting.
        if (oldstatus & STATUS_NEEDS_WAKEUP) != 0 {
            std::sync::atomic::fence(Ordering::Release);
            self.sema_.post();
        }
    }

    /// Apply any pending updates to the data.
    /// This accepts data because it must be locked.
    /// \return the updated generation list.
    fn updated_gens_in_data(&self, data: &mut MutexGuard<data_t>) -> GenerationsList {
        // Atomically acquire the pending updates, swapping in 0.
        // If there are no pending updates (likely) or a thread is waiting, just return.
        // Otherwise CAS in 0 and update our topics.
        const relaxed: Ordering = Ordering::Relaxed;
        let mut changed_topic_bits: topic_bitmask_t = 0;
        let mut cas_success = false;
        while !cas_success {
            changed_topic_bits = self.status_.load(relaxed);
            if changed_topic_bits == 0 || changed_topic_bits == STATUS_NEEDS_WAKEUP {
                return data.current.clone();
            }
            cas_success = self
                .status_
                .compare_exchange_weak(changed_topic_bits, 0, relaxed, relaxed)
                .is_ok();
        }
        assert!(
            (changed_topic_bits & STATUS_NEEDS_WAKEUP) == 0,
            "Thread waiting bit should not be set"
        );

        // Update the current generation with our topics and return it.
        for topic in all_topics() {
            if changed_topic_bits & topic_to_bit(topic) != 0 {
                data.current.set(topic, data.current.get(topic) + 1);
                FLOG!(
                    topic_monitor,
                    "Updating topic",
                    topic,
                    "to",
                    data.current.get(topic)
                );
            }
        }
        // Report our change.
        self.data_notifier_.notify_all();
        return data.current.clone();
    }

    /// \return the current generation list, opportunistically applying any pending updates.
    fn updated_gens(&self) -> GenerationsList {
        let mut data = self.data_.lock().unwrap();
        return self.updated_gens_in_data(&mut data);
    }

    /// Access the current generations.
    pub fn current_generations(self: &topic_monitor_t) -> GenerationsList {
        self.updated_gens()
    }

    /// Access the generation for a topic.
    pub fn generation_for_topic(self: &topic_monitor_t, topic: topic_t) -> generation_t {
        self.current_generations().get(topic)
    }

    /// Given a list of input generations, attempt to update them to something newer.
    /// If \p gens is older, then just return those by reference, and directly return false (not
    /// becoming the reader).
    /// If \p gens is current and there is not a reader, then do not update \p gens and return true,
    /// indicating we should become the reader. Now it is our responsibility to wait on the
    /// semaphore and notify on a change via the condition variable. If \p gens is current, and
    /// there is already a reader, then wait until the reader notifies us and try again.
    fn try_update_gens_maybe_becoming_reader(&self, gens: &mut GenerationsList) -> bool {
        let mut become_reader = false;
        let mut data = self.data_.lock().unwrap();
        loop {
            // See if the updated gen list has changed. If so we don't need to become the reader.
            let current = self.updated_gens_in_data(&mut data);
            // FLOG(topic_monitor, "TID", thread_id(), "local ", gens->describe(), ": current",
            //      current.describe());
            if *gens != current {
                *gens = current;
                break;
            }

            // The generations haven't changed. Perhaps we become the reader.
            // Note we still hold the lock, so this cannot race with any other thread becoming the
            // reader.
            if data.has_reader {
                // We already have a reader, wait for it to notify us and loop again.
                data = self.data_notifier_.wait(data).unwrap();
                continue;
            } else {
                // We will try to become the reader.
                // Reader bit should not be set in this case.
                assert!(
                    (self.status_.load(Ordering::Relaxed) & STATUS_NEEDS_WAKEUP) == 0,
                    "No thread should be waiting"
                );
                // Try becoming the reader by marking the reader bit.
                let expected_old: status_bits_t = 0;
                if self
                    .status_
                    .compare_exchange(
                        expected_old,
                        STATUS_NEEDS_WAKEUP,
                        Ordering::SeqCst,
                        Ordering::SeqCst,
                    )
                    .is_err()
                {
                    // We failed to become the reader, perhaps because another topic post just arrived.
                    // Loop again.
                    continue;
                }
                // We successfully did a CAS from 0 -> STATUS_NEEDS_WAKEUP.
                // Now any successive topic post must signal us.
                //FLOG(topic_monitor, "TID", thread_id(), "becoming reader");
                become_reader = true;
                data.has_reader = true;
                break;
            }
        }
        return become_reader;
    }

    /// Wait for some entry in the list of generations to change.
    /// \return the new gens.
    fn await_gens(&self, input_gens: &GenerationsList) -> GenerationsList {
        let mut gens = input_gens.clone();
        while &gens == input_gens {
            let become_reader = self.try_update_gens_maybe_becoming_reader(&mut gens);
            if become_reader {
                // Now we are the reader. Read from the pipe, and then update with any changes.
                // Note we no longer hold the lock.
                assert!(
                    gens == *input_gens,
                    "Generations should not have changed if we are the reader."
                );

                // Wait to be woken up.
                self.sema_.wait();

                // We are finished waiting. We must stop being the reader, and post on the condition
                // variable to wake up any other threads waiting for us to finish reading.
                let mut data = self.data_.lock().unwrap();
                gens = data.current.clone();
                // FLOG(topic_monitor, "TID", thread_id(), "local", input_gens.describe(),
                //      "read() complete, current is", gens.describe());
                assert!(data.has_reader, "We should be the reader");
                data.has_reader = false;
                self.data_notifier_.notify_all();
            }
        }
        return gens;
    }

    /// For each valid topic in \p gens, check to see if the current topic is larger than
    /// the value in \p gens.
    /// If \p wait is set, then wait if there are no changes; otherwise return immediately.
    /// \return true if some topic changed, false if none did.
    /// On a true return, this updates the generation list \p gens.
    pub fn check(&self, gens: &GenerationsList, wait: bool) -> bool {
        if !gens.any_valid() {
            return false;
        }

        let mut current: GenerationsList = self.updated_gens();
        let mut changed = false;
        loop {
            // Load the topic list and see if anything has changed.
            for topic in all_topics() {
                if gens.is_valid(topic) {
                    assert!(
                        gens.get(topic) <= current.get(topic),
                        "Incoming gen count exceeded published count"
                    );
                    if gens.get(topic) < current.get(topic) {
                        gens.set(topic, current.get(topic));
                        changed = true;
                    }
                }
            }

            // If we're not waiting, or something changed, then we're done.
            if !wait || changed {
                break;
            }

            // Wait until our gens change.
            current = self.await_gens(&current);
        }
        return changed;
    }
}

pub fn topic_monitor_init() {
    topic_monitor_t::initialize();
}

pub fn topic_monitor_principal() -> &'static topic_monitor_t {
    unsafe {
        assert!(
            !s_principal.is_null(),
            "Principal topic monitor not initialized"
        );
        &*s_principal
    }
}

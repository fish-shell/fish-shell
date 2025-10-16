//! This module houses `TimerSnapshot` which can be used to calculate the elapsed time (system CPU
//! time, user CPU time, and observed wall time, broken down by fish and child processes spawned by
//! fish) between two `TimerSnapshot` instances.
//!
//! Measuring time is always complicated with many caveats. Quite apart from the typical
//! gotchas faced by developers attempting to choose between monotonic vs non-monotonic and system vs
//! cpu clocks, the fact that we are executing as a shell further complicates matters: we can't just
//! observe the elapsed CPU time, because that does not reflect the total execution time for both
//! ourselves (internal shell execution time and the time it takes for builtins and functions to
//! execute) and any external processes we spawn.
//!
//! `std::time::Instant` is used to monitor elapsed wall time. Unlike `SystemTime`, `Instant` is
//! guaranteed to be monotonic though it is likely to not be as high of a precision as we would like
//! but it's still the best we can do because we don't know how long of a time might elapse between
//! `TimerSnapshot` instances and need to avoid rollover.

use std::fmt::Write as FmtWrite;
use std::io::Write;
use std::time::{Duration, Instant};

use crate::nix::{RUsage, getrusage};

enum Unit {
    Minutes,
    Seconds,
    Millis,
    Micros,
}

struct TimerSnapshot {
    wall_time: Instant,
    cpu_fish: libc::rusage,
    cpu_children: libc::rusage,
}

/// Create a `TimerSnapshot` and return a `PrintElapsedOnDrop` object that will print upon
/// being dropped the delta between now and the time that it is dropped at.
pub fn push_timer() -> PrintElapsedOnDrop {
    PrintElapsedOnDrop {
        start: TimerSnapshot::take(),
    }
}

impl TimerSnapshot {
    pub fn take() -> TimerSnapshot {
        TimerSnapshot {
            cpu_fish: getrusage(RUsage::RSelf),
            cpu_children: getrusage(RUsage::RChildren),
            wall_time: Instant::now(),
        }
    }

    /// Returns a formatted string containing the detailed difference between two `TimerSnapshot`
    /// instances. The returned string can take one of two formats, depending on the value of the
    /// `verbose` parameter.
    pub fn get_delta(t1: &TimerSnapshot, t2: &TimerSnapshot, verbose: bool) -> String {
        use crate::nix::timeval_to_duration as from;

        let mut fish_sys = from(&t2.cpu_fish.ru_stime) - from(&t1.cpu_fish.ru_stime);
        let mut fish_usr = from(&t2.cpu_fish.ru_utime) - from(&t1.cpu_fish.ru_utime);
        let mut child_sys = from(&t2.cpu_children.ru_stime) - from(&t1.cpu_children.ru_stime);
        let mut child_usr = from(&t2.cpu_children.ru_utime) - from(&t1.cpu_children.ru_utime);

        // The result from getrusage is not necessarily realtime, it may be cached from a few
        // microseconds ago. In the event that execution completes extremely quickly or there is
        // no data (say, we are measuring external execution time but no external processes have
        // been launched), it can incorrectly appear to be negative.
        fish_sys = fish_sys.max(Duration::ZERO);
        fish_usr = fish_usr.max(Duration::ZERO);
        child_sys = child_sys.max(Duration::ZERO);
        child_usr = child_usr.max(Duration::ZERO);
        // As `Instant` is strictly monotonic, this can't be negative so we don't need to clamp.
        let net_wall_micros = (t2.wall_time - t1.wall_time).as_micros() as i64;
        let net_sys_micros = (fish_sys + child_sys).as_micros() as i64;
        let net_usr_micros = (fish_usr + child_usr).as_micros() as i64;

        let wall_unit = Unit::for_micros(net_wall_micros);
        // Make sure we share the same unit for the various CPU times
        let cpu_unit = Unit::for_micros(net_sys_micros.max(net_usr_micros));

        let wall_time = wall_unit.convert_micros(net_wall_micros);
        let sys_time = cpu_unit.convert_micros(net_sys_micros);
        let usr_time = cpu_unit.convert_micros(net_usr_micros);

        let mut output = String::new();
        #[rustfmt::skip]
        if !verbose {
            write!(output, "\n_______________________________").unwrap();
            write!(output, "\nExecuted in  {:6.2} {}", wall_time, wall_unit.long_name()).unwrap();
            write!(output, "\n   usr time  {:6.2} {}", usr_time, cpu_unit.long_name()).unwrap();
            write!(output, "\n   sys time  {:6.2} {}", sys_time, cpu_unit.long_name()).unwrap();
        } else {
            let fish_unit = Unit::for_micros(fish_sys.max(fish_usr).as_micros() as i64);
            let child_unit = Unit::for_micros(child_sys.max(child_usr).as_micros() as i64);
            let fish_usr_time = fish_unit.convert_micros(fish_usr.as_micros() as i64);
            let fish_sys_time = fish_unit.convert_micros(fish_sys.as_micros() as i64);
            let child_usr_time = child_unit.convert_micros(child_usr.as_micros() as i64);
            let child_sys_time = child_unit.convert_micros(child_sys.as_micros() as i64);

            let column2_unit_len = wall_unit
                .short_name()
                .len()
                .max(cpu_unit.short_name().len());
            let wall_unit = wall_unit.short_name();
            let cpu_unit = cpu_unit.short_name();
            let fish_unit = fish_unit.short_name();
            let child_unit = child_unit.short_name();

            output += "\n________________________________________________________";
            write!(
                output,
                "\nExecuted in  {wall_time:6.2} {wall_unit:<width1$}    {fish:<width2$}  external",
                width1 = column2_unit_len,
                fish = "fish",
                width2 = fish_unit.len() + 7
            )
            .unwrap();
            write!(output, "\n   usr time  {usr_time:6.2} {cpu_unit:<column2_unit_len$}  {fish_usr_time:6.2} {fish_unit}  {child_usr_time:6.2} {child_unit}").unwrap();
            write!(output, "\n   sys time  {sys_time:6.2} {cpu_unit:<column2_unit_len$}  {fish_sys_time:6.2} {fish_unit}  {child_sys_time:6.2} {child_unit}").unwrap();
        };
        output += "\n";

        output
    }
}

/// When dropped, prints to stderr the time that has elapsed since it was initialized.
pub struct PrintElapsedOnDrop {
    start: TimerSnapshot,
}

impl Drop for PrintElapsedOnDrop {
    fn drop(&mut self) {
        let end = TimerSnapshot::take();

        // Well, this is awkward. By defining `time` as a decorator and not a built-in, there's
        // no associated stream for its output!
        let output = TimerSnapshot::get_delta(&self.start, &end, true);
        let mut stderr = std::io::stderr().lock();
        // There is no bubbling up of errors in a Drop implementation, and it's absolutely forbidden
        // to panic.
        let _ = stderr.write_all(output.as_bytes());
        let _ = stderr.write_all(b"\n");
    }
}

impl Unit {
    /// Return the appropriate unit to format the provided number of microseconds in.
    const fn for_micros(micros: i64) -> Unit {
        match micros {
            900_000_001.. => Unit::Minutes,
            // Move to seconds if we would overflow the %6.2 format
            999_995.. => Unit::Seconds,
            1000.. => Unit::Millis,
            _ => Unit::Micros,
        }
    }

    const fn short_name(&self) -> &'static str {
        match *self {
            Unit::Minutes => "mins",
            Unit::Seconds => "secs",
            Unit::Millis => "millis",
            Unit::Micros => "micros",
        }
    }

    const fn long_name(&self) -> &'static str {
        match *self {
            Unit::Minutes => "minutes",
            Unit::Seconds => "seconds",
            Unit::Millis => "milliseconds",
            Unit::Micros => "microseconds",
        }
    }

    fn convert_micros(&self, micros: i64) -> f64 {
        match *self {
            Unit::Minutes => micros as f64 / 1.0E6 / 60.0,
            Unit::Seconds => micros as f64 / 1.0E6,
            Unit::Millis => micros as f64 / 1.0E3,
            Unit::Micros => micros as f64 / 1.0,
        }
    }
}

#[test]
fn timer_format_and_alignment() {
    let mut t1 = TimerSnapshot::take();
    t1.cpu_fish.ru_utime.tv_usec = 0;
    t1.cpu_fish.ru_stime.tv_usec = 0;
    t1.cpu_children.ru_utime.tv_usec = 0;
    t1.cpu_children.ru_stime.tv_usec = 0;

    let mut t2 = TimerSnapshot::take();
    t2.cpu_fish.ru_utime.tv_usec = 999995;
    t2.cpu_fish.ru_stime.tv_usec = 999994;
    t2.cpu_children.ru_utime.tv_usec = 1000;
    t2.cpu_children.ru_stime.tv_usec = 500;
    t2.wall_time = t1.wall_time + Duration::from_micros(500);

    let expected = r#"
________________________________________________________
Executed in  500.00 micros    fish         external
   usr time    1.00 secs      1.00 secs    1.00 millis
   sys time    1.00 secs      1.00 secs    0.50 millis
"#;
    //        (a)            (b)            (c)
    // (a) remaining columns should align even if there are different units
    // (b) carry to the next unit when it would overflow %6.2F
    // (c) carry to the next unit when the larger one exceeds 1000
    let actual = TimerSnapshot::get_delta(&t1, &t2, true);
    assert_eq!(actual, expected);
}

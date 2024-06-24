use crate::tests::prelude::*;
use crate::topic_monitor::{GenerationsList, Topic, TopicMonitor};
use std::sync::{
    atomic::{AtomicU32, AtomicU64, Ordering},
    Arc,
};

#[test]
#[serial]
fn test_topic_monitor() {
    let _cleanup = test_init();
    let monitor = TopicMonitor::default();
    let gens = GenerationsList::new();
    let t = Topic::sigchld;
    gens.sigchld.set(0);
    assert_eq!(monitor.generation_for_topic(t), 0);
    let changed = monitor.check(&gens, false /* wait */);
    assert!(!changed);
    assert_eq!(gens.sigchld.get(), 0);

    monitor.post(t);
    let changed = monitor.check(&gens, true /* wait */);
    assert!(changed);
    assert_eq!(gens.get(t), 1);
    assert_eq!(monitor.generation_for_topic(t), 1);

    monitor.post(t);
    assert_eq!(monitor.generation_for_topic(t), 2);
    let changed = monitor.check(&gens, true /* wait */);
    assert!(changed);
    assert_eq!(gens.sigchld.get(), 2);
}

#[test]
#[serial]
fn test_topic_monitor_torture() {
    let _cleanup = test_init();
    let monitor = Arc::new(TopicMonitor::default());
    const THREAD_COUNT: usize = 64;
    let t1 = Topic::sigchld;
    let t2 = Topic::sighupint;
    let mut gens_list = vec![GenerationsList::invalid(); THREAD_COUNT];
    let post_count = Arc::new(AtomicU64::new(0));
    for gen in &mut gens_list {
        *gen = monitor.current_generations();
        post_count.fetch_add(1, Ordering::Relaxed);
        monitor.post(t1);
    }

    let completed = Arc::new(AtomicU32::new(0));
    let mut threads = vec![];

    for gens in gens_list {
        let monitor = Arc::downgrade(&monitor);
        let post_count = Arc::downgrade(&post_count);
        let completed = Arc::downgrade(&completed);
        threads.push(std::thread::spawn(move || {
            for _ in 0..1 << 11 {
                let before = gens.clone();
                let _changed = monitor.upgrade().unwrap().check(&gens, true /* wait */);
                assert!(before.get(t1) < gens.get(t1));
                assert!(gens.get(t1) <= post_count.upgrade().unwrap().load(Ordering::Relaxed));
                assert_eq!(gens.get(t2), 0);
            }
            let _amt = completed.upgrade().unwrap().fetch_add(1, Ordering::Relaxed);
        }));
    }

    while completed.load(Ordering::Relaxed) < THREAD_COUNT.try_into().unwrap() {
        post_count.fetch_add(1, Ordering::Relaxed);
        monitor.post(t1);
        std::thread::yield_now();
    }
    for t in threads {
        t.join().unwrap();
    }
}

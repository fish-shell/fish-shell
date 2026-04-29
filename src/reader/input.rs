//! Reader implementation of InputEventQueuer.
use super::{Reader, reader_reading_interrupted, reader_schedule_prompt_repaint};
use crate::{
    event,
    input::input_get_bind_mode,
    input_common::{CharEvent, InputData, InputEventQueuer, ReadlineCmd},
    proc::job_reap,
    signal::signal_clear_cancel,
};
use fish_common::escape;
use fish_widestring::{WString, bytes2wcstring};
use std::os::fd::RawFd;

impl<'a> InputEventQueuer for Reader<'a> {
    fn get_input_data(&self) -> &InputData {
        &self.input_data
    }

    fn get_input_data_mut(&mut self) -> &mut InputData {
        &mut self.input_data
    }

    fn get_ioport_fd(&self) -> RawFd {
        self.debouncers.event_signaller_read_fd()
    }

    fn prepare_to_select(&mut self) {
        // Fire any pending events and reap stray processes, including printing exit status messages.
        event::fire_delayed(self.parser);
        if job_reap(self.parser, true, None) {
            reader_schedule_prompt_repaint();
        }
    }

    fn select_interrupted(&mut self) {
        // Readline commands may be bound to \cc which also sets the cancel flag.
        // See #6937, #8125.
        signal_clear_cancel();

        // Fire any pending events and reap stray processes, including printing exit status messages.
        let parser = &mut *self.parser;
        event::fire_delayed(parser);
        if job_reap(parser, true, None) {
            reader_schedule_prompt_repaint();
        }

        // Tell the reader an event occurred.
        if reader_reading_interrupted(self) != 0 {
            self.enqueue_interrupt_key();
            return;
        }
        self.push_front(CharEvent::from_check_exit());
    }

    fn uvar_change_notified(&mut self) {
        self.parser.sync_uvars_and_fire(true /* always */);
    }

    fn ioport_notified(&mut self) {
        // Our iothread signaller was posted, indicating some debouncer has a new result.
        self.debouncers.event_signaller.try_consume();
        self.service_debounced_results();
    }

    fn paste_start_buffering(&mut self) {
        self.input_data.paste_buffer = Some(vec![]);
        self.push_front(CharEvent::from_readline(ReadlineCmd::BeginUndoGroup));
    }

    fn paste_commit(&mut self) {
        self.push_front(CharEvent::from_readline(ReadlineCmd::EndUndoGroup));
        let Some(buffer) = self.input_data.paste_buffer.take() else {
            return;
        };
        self.push_front(CharEvent::Command(sprintf!(
            "__fish_paste %s",
            escape(&bytes2wcstring(&buffer))
        )));
    }

    fn get_bind_mode(&self) -> WString {
        input_get_bind_mode(self.parser.vars())
    }
}

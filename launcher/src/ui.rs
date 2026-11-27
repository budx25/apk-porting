//! The progress window. It only appears when there is something to wait for
//! (installing, downloading, applying) or an error to show.
use crate::update::Progress;
use eframe::egui;
use std::sync::mpsc::{channel, Receiver, Sender};
use std::sync::{Arc, Mutex};
use std::time::Duration;

pub enum Msg {
    Status(&'static str),
    Progress(Progress),
    Done(Result<(), String>),
}

/// A re-runnable unit of work (so "Retry" can start it again).
pub type Job = dyn Fn(&Sender<Msg>) + Send + Sync;

pub fn spawn(job: &Arc<Job>) -> Receiver<Msg> {
    let (tx, rx) = channel();
    let job = Arc::clone(job);
    std::thread::spawn(move || job(&tx));
    rx
}

/// Waits without a window. Returns the result if the job finished without
/// anything worth showing; returns `None` (with the messages put back) when
/// a window is needed.
pub fn wait_quietly(rx: Receiver<Msg>) -> Result<(), (Receiver<Msg>, Vec<Msg>)> {
    let mut seen = Vec::new();
    loop {
        match rx.recv() {
            Ok(Msg::Progress(Progress::Checking)) => continue,
            Ok(Msg::Done(Ok(()))) => return Ok(()),
            Ok(other) => {
                seen.push(other);
                return Err((rx, seen));
            }
            Err(_) => {
                seen.push(Msg::Done(Err("the update worker stopped unexpectedly".into())));
                return Err((rx, seen));
            }
        }
    }
}

struct App {
    job: Arc<Job>,
    rx: Receiver<Msg>,
    pending: Vec<Msg>,
    status: String,
    fraction: Option<f32>,
    error: Option<String>,
    play: Arc<Mutex<bool>>,
}

impl App {
    fn apply(&mut self, msg: Msg, ctx: &egui::Context) {
        match msg {
            Msg::Status(s) => (self.status, self.fraction) = (s.to_string(), None),
            Msg::Progress(Progress::Checking) => (self.status, self.fraction) = ("Checking for updates…".into(), None),
            Msg::Progress(Progress::Downloading { done, total }) => {
                self.status = format!("Downloading update… {:.0} / {:.0} MB", done as f64 / 1e6, total as f64 / 1e6);
                self.fraction = Some(if total == 0 { 0.0 } else { done as f32 / total as f32 });
            }
            Msg::Progress(Progress::Applying) => (self.status, self.fraction) = ("Applying update…".into(), None),
            Msg::Done(Ok(())) => {
                *self.play.lock().unwrap() = true;
                ctx.send_viewport_cmd(egui::ViewportCommand::Close);
            }
            Msg::Done(Err(e)) => self.error = Some(e),
        }
    }
}

impl eframe::App for App {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        for msg in std::mem::take(&mut self.pending) {
            self.apply(msg, ctx);
        }
        while let Ok(msg) = self.rx.try_recv() {
            self.apply(msg, ctx);
        }
        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading("Ninja Heroes");
            if let Some(err) = self.error.clone() {
                ui.label(format!("Couldn't update: {err}"));
                ui.horizontal(|ui| {
                    if ui.button("Retry").clicked() {
                        self.error = None;
                        self.rx = spawn(&self.job);
                    }
                    if ui.button("Play anyway").clicked() {
                        *self.play.lock().unwrap() = true;
                        ctx.send_viewport_cmd(egui::ViewportCommand::Close);
                    }
                });
            } else {
                ui.label(&self.status);
                match self.fraction {
                    Some(f) => ui.add(egui::ProgressBar::new(f).show_percentage()),
                    None => ui.spinner(),
                };
            }
        });
        ctx.request_repaint_after(Duration::from_millis(100));
    }
}

/// Shows the window until the job succeeds or the owner chooses. Returns whether to start the game.
pub fn show(job: Arc<Job>, rx: Receiver<Msg>, pending: Vec<Msg>) -> bool {
    let play = Arc::new(Mutex::new(false));
    let app = App { job, rx, pending, status: "Starting…".into(), fraction: None, error: None, play: Arc::clone(&play) };
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default().with_inner_size([440.0, 130.0]).with_resizable(false),
        ..Default::default()
    };
    let _ = eframe::run_native("Ninja Heroes", options, Box::new(|_| Ok(Box::new(app))));
    let started = *play.lock().unwrap();
    started
}

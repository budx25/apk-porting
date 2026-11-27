#![cfg_attr(not(test), windows_subsystem = "windows")]

mod extract;
mod http;
mod install;
mod launch;
mod paths;
mod settings;
mod ui;
mod update;
mod urls;
#[cfg(test)]
mod testzip;
mod version;

use paths::Paths;
use settings::Settings;
use std::io::Write;
use std::path::PathBuf;
use std::sync::Arc;
use ui::{Job, Msg};

fn log(p: &Paths, msg: &str) {
    let secs = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0);
    if let Ok(mut f) = std::fs::OpenOptions::new().create(true).append(true).open(p.logs.join("launcher.log")) {
        let _ = writeln!(f, "{secs} {msg}");
    }
}

fn apk_from_args() -> Option<PathBuf> {
    let mut args = std::env::args_os().skip(1);
    while let Some(a) = args.next() {
        if a == "--apk" {
            return args.next().map(PathBuf::from);
        }
    }
    None
}

fn launch_game(p: &Paths, s: &Settings) {
    if !launch::game_exe(p).is_file() {
        log(p, "game not installed");
        rfd::MessageDialog::new()
            .set_title("Ninja Heroes")
            .set_description("The game itself isn't installed yet. Build it with port\\build.ps1 -Install, then start the launcher again.")
            .show();
        return;
    }
    match launch::game_command(p, s).spawn() {
        Ok(_) => log(p, "game started"),
        Err(e) => log(p, &format!("game failed to start: {e}")),
    }
}

fn main() {
    let p = Paths::new(Paths::default_root());
    let _ = std::fs::create_dir_all(&p.logs);
    log(&p, "launcher start");

    if launch::game_running() {
        log(&p, "game already running; focusing it");
        launch::focus_game();
        return;
    }
    let settings = Settings::load(&p.settings);

    let apk = if p.game.join("VERSION").is_file() {
        None
    } else {
        match apk_from_args().or_else(|| rfd::FileDialog::new().set_title("Choose the Ninja Heroes APK").add_filter("Android app", &["apk"]).pick_file()) {
            Some(a) => Some(a),
            None => return,
        }
    };

    let jp = p.clone();
    let job: Arc<Job> = Arc::new(move |tx| {
        let result = (|| -> anyhow::Result<()> {
            if let Some(apk) = &apk {
                if !jp.game.join("VERSION").is_file() {
                    let _ = tx.send(Msg::Status("Installing the game files from the APK…"));
                    install::install_from_apk(&jp, apk)?;
                }
            }
            let outcome = update::run(&jp, &http::UreqHttp::new(), &mut |pr| {
                let _ = tx.send(Msg::Progress(pr));
            })?;
            log(&jp, &format!("update: {outcome:?}"));
            Ok(())
        })();
        if let Err(e) = &result {
            log(&jp, &format!("update failed: {e:#}"));
        }
        let _ = tx.send(Msg::Done(result.map_err(|e| format!("{e:#}"))));
    });

    let play = match ui::wait_quietly(ui::spawn(&job)) {
        Ok(()) => true,
        Err((rx, seen)) => ui::show(job, rx, seen),
    };
    if play {
        launch_game(&p, &settings);
    }
}

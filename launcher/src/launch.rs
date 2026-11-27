//! Starting the game, and noticing when it already runs.
use crate::paths::Paths;
use crate::settings::Settings;
use std::process::Command;

/// Held by the game while it runs (see port/app/proj.win32/main.cpp).
pub const GAME_MUTEX: &str = r"Local\NinjaHeroesPC.Game";
const GAME_TITLE: &str = "Ninja Heroes";

fn wide(s: &str) -> Vec<u16> {
    s.encode_utf16().chain(Some(0)).collect()
}

pub fn game_exe(p: &Paths) -> std::path::PathBuf {
    p.bin.join("NinjaHeroes.exe")
}

pub fn game_command(p: &Paths, s: &Settings) -> Command {
    let mut cmd = Command::new(game_exe(p));
    cmd.arg("--width").arg(s.width.to_string()).arg("--height").arg(s.height.to_string());
    if s.fullscreen {
        cmd.arg("--fullscreen");
    }
    cmd.arg("--log").arg(p.logs.join("game.log")).current_dir(&p.game);
    cmd
}

pub fn game_running() -> bool {
    use windows_sys::Win32::Foundation::CloseHandle;
    use windows_sys::Win32::System::Threading::{OpenMutexW, SYNCHRONIZATION_SYNCHRONIZE};
    let name = wide(GAME_MUTEX);
    let h = unsafe { OpenMutexW(SYNCHRONIZATION_SYNCHRONIZE, 0, name.as_ptr()) };
    if h.is_null() {
        return false;
    }
    unsafe { CloseHandle(h) };
    true
}

pub fn focus_game() {
    use windows_sys::Win32::UI::WindowsAndMessaging::{FindWindowW, SetForegroundWindow, ShowWindow, SW_RESTORE};
    let title = wide(GAME_TITLE);
    let hwnd = unsafe { FindWindowW(std::ptr::null(), title.as_ptr()) };
    if !hwnd.is_null() {
        unsafe {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::paths::Paths;
    use crate::settings::Settings;

    #[test]
    fn game_command_args() {
        let p = Paths::new(std::path::PathBuf::from(r"C:\data\NinjaHeroesPC"));
        let cmd = game_command(&p, &Settings { width: 1136, height: 640, fullscreen: false });
        let args: Vec<String> = cmd.get_args().map(|a| a.to_string_lossy().into_owned()).collect();
        assert_eq!(args, ["--width", "1136", "--height", "640", "--log", r"C:\data\NinjaHeroesPC\logs\game.log"]);
        assert_eq!(cmd.get_program(), std::ffi::OsStr::new(r"C:\data\NinjaHeroesPC\bin\NinjaHeroes.exe"));
        assert_eq!(cmd.get_current_dir(), Some(p.game.as_path()));
        let full = game_command(&p, &Settings { width: 800, height: 600, fullscreen: true });
        assert!(full.get_args().any(|a| a == "--fullscreen"));
    }

    #[test]
    fn game_running_detects_mutex() {
        use windows_sys::Win32::Foundation::CloseHandle;
        use windows_sys::Win32::System::Threading::CreateMutexW;
        assert!(!game_running());
        let name: Vec<u16> = GAME_MUTEX.encode_utf16().chain(Some(0)).collect();
        let h = unsafe { CreateMutexW(std::ptr::null(), 0, name.as_ptr()) };
        assert!(!h.is_null());
        assert!(game_running());
        unsafe { CloseHandle(h) };
        assert!(!game_running());
    }
}

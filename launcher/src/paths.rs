//! Layout of the launcher's data folder.
use std::path::PathBuf;

#[derive(Debug, Clone)]
pub struct Paths {
    pub root: PathBuf,
    pub game: PathBuf,
    pub bin: PathBuf,
    pub logs: PathBuf,
    pub staging: PathBuf,
    pub settings: PathBuf,
}

impl Paths {
    pub fn new(root: PathBuf) -> Paths {
        Paths {
            game: root.join("game"),
            bin: root.join("bin"),
            logs: root.join("logs"),
            staging: root.join("staging"),
            settings: root.join("launcher.toml"),
            root,
        }
    }

    /// `%LOCALAPPDATA%\NinjaHeroesPC`
    pub fn default_root() -> PathBuf {
        let base = std::env::var_os("LOCALAPPDATA").map(PathBuf::from).unwrap_or_else(|| PathBuf::from("."));
        base.join("NinjaHeroesPC")
    }
}

//! `launcher.toml`: the few things the owner can set.
use serde::Deserialize;
use std::path::Path;

#[derive(Debug, Clone, Deserialize)]
#[serde(default)]
pub struct Settings {
    pub width: u32,
    pub height: u32,
    pub fullscreen: bool,
}

impl Default for Settings {
    fn default() -> Settings {
        Settings { width: 1136, height: 640, fullscreen: false }
    }
}

impl Settings {
    /// Defaults when the file is missing or invalid.
    pub fn load(path: &Path) -> Settings {
        std::fs::read_to_string(path).ok().and_then(|t| toml::from_str(&t).ok()).unwrap_or_default()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn settings_defaults_when_missing() {
        let dir = tempfile::tempdir().unwrap();
        let s = Settings::load(&dir.path().join("launcher.toml"));
        assert_eq!((s.width, s.height, s.fullscreen), (1136, 640, false));
    }

    #[test]
    fn settings_defaults_when_invalid_and_round_trip() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("launcher.toml");
        std::fs::write(&path, "width = \"wide\"").unwrap();
        assert_eq!(Settings::load(&path).width, 1136);
        std::fs::write(&path, "width = 1600\nheight = 900\nfullscreen = true\n").unwrap();
        let s = Settings::load(&path);
        assert_eq!((s.width, s.height, s.fullscreen), (1600, 900, true));
    }
}

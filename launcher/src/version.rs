//! The game's `VERSION` file: `{"v","c","s","ch","d"}` (version, CDN, server domain, channel, debug).
use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use std::path::Path;

#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct VersionFile {
    pub v: String,
    pub c: String,
    pub s: String,
    pub ch: String,
    pub d: String,
}

pub fn read_version(path: &Path) -> Result<VersionFile> {
    let text = std::fs::read_to_string(path).with_context(|| format!("reading {}", path.display()))?;
    serde_json::from_str(&text).with_context(|| format!("parsing {}", path.display()))
}

pub fn set_version(path: &Path, new_v: &str) -> Result<()> {
    let mut vf = read_version(path)?;
    vf.v = new_v.to_string();
    std::fs::write(path, serde_json::to_string(&vf)?).with_context(|| format!("writing {}", path.display()))
}

#[cfg(test)]
mod tests {
    use super::*;

    const SAMPLE: &str = r#"{"v":"2.6.1","c":"patch.kageherostudio.com","s":"kageherostudio.com","ch":"108","d":"false"}"#;

    #[test]
    fn read_version_parses_fields() {
        let dir = tempfile::tempdir().unwrap();
        let p = dir.path().join("VERSION");
        std::fs::write(&p, SAMPLE).unwrap();
        let vf = read_version(&p).unwrap();
        assert_eq!(vf.v, "2.6.1");
        assert_eq!(vf.ch, "108");
    }

    #[test]
    fn set_version_changes_only_v() {
        let dir = tempfile::tempdir().unwrap();
        let p = dir.path().join("VERSION");
        std::fs::write(&p, SAMPLE).unwrap();
        set_version(&p, "2.7.0").unwrap();
        let vf = read_version(&p).unwrap();
        assert_eq!(vf.v, "2.7.0");
        assert_eq!(vf.ch, "108");
        assert_eq!(vf.c, "patch.kageherostudio.com");
    }
}

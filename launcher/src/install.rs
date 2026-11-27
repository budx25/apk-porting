//! First-run install from the APK, and applying hot-update patches.
use crate::extract::{extract_zip, move_over};
use crate::paths::Paths;
use crate::version::set_version;
use anyhow::{bail, Result};
use std::path::Path;

fn clear_staging(p: &Paths) -> Result<()> {
    if p.staging.exists() {
        std::fs::remove_dir_all(&p.staging)?;
    }
    Ok(())
}

/// Copies the APK's `assets/` into `game\`. Refuses an APK without `assets/VERSION`.
pub fn install_from_apk(p: &Paths, apk: &Path) -> Result<()> {
    clear_staging(p)?;
    let result = extract_zip(apk, &p.staging, "assets/");
    if result.is_err() || !p.staging.join("VERSION").is_file() {
        clear_staging(p)?;
        result?;
        bail!("{} is not the Ninja Heroes APK (no assets/VERSION)", apk.display());
    }
    move_over(&p.staging, &p.game)
}

/// Overlays a patch zip onto `game\`. `VERSION` is written last, so an interrupted
/// apply keeps the old version and the next run re-applies the same patch.
pub fn apply_patch(p: &Paths, zip: &Path, new_v: &str) -> Result<()> {
    clear_staging(p)?;
    if let Err(e) = extract_zip(zip, &p.staging, "") {
        clear_staging(p)?;
        return Err(e);
    }
    let _ = std::fs::remove_file(p.staging.join(".gitignore"));
    let patch_version = p.staging.join("VERSION");
    let held = p.root.join("VERSION.new");
    if patch_version.is_file() {
        std::fs::rename(&patch_version, &held)?;
    }
    move_over(&p.staging, &p.game)?;
    if held.is_file() {
        std::fs::rename(&held, p.game.join("VERSION"))?;
    }
    set_version(&p.game.join("VERSION"), new_v)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::paths::Paths;
    use crate::testzip::make_zip;
    use crate::version::read_version;

    const V111: &[u8] = br#"{"v":"1.1.1","c":"patch.kageherostudio.com","s":"kageherostudio.com","ch":"108","d":"false"}"#;
    const V261: &[u8] = br#"{"v":"2.6.1","c":"patch.kageherostudio.com","s":"kageherostudio.com","ch":"108","d":"false"}"#;

    fn game_with_v111(p: &Paths) {
        std::fs::create_dir_all(p.game.join("UI")).unwrap();
        std::fs::write(p.game.join("VERSION"), V111).unwrap();
        std::fs::write(p.game.join("UI/a.png"), b"old").unwrap();
    }

    #[test]
    fn install_from_apk_copies_assets() {
        let dir = tempfile::tempdir().unwrap();
        let p = Paths::new(dir.path().join("root"));
        let apk = dir.path().join("game.apk");
        make_zip(&apk, &[("assets/VERSION", V111), ("assets/UI/main.jsc", b"js"), ("lib/x.so", b"so")]);
        install_from_apk(&p, &apk).unwrap();
        assert_eq!(read_version(&p.game.join("VERSION")).unwrap().v, "1.1.1");
        assert!(p.game.join("UI/main.jsc").is_file());
        assert!(!p.staging.exists());
    }

    #[test]
    fn install_rejects_wrong_apk() {
        let dir = tempfile::tempdir().unwrap();
        let p = Paths::new(dir.path().join("root"));
        let apk = dir.path().join("other.apk");
        make_zip(&apk, &[("assets/other.txt", b"x"), ("classes.dex", b"dex")]);
        let err = install_from_apk(&p, &apk).unwrap_err();
        assert!(format!("{err:#}").contains("not the Ninja Heroes APK"), "{err:#}");
        assert!(!p.game.exists());
        let not_zip = dir.path().join("notes.txt");
        std::fs::write(&not_zip, b"hello").unwrap();
        assert!(install_from_apk(&p, &not_zip).is_err());
        assert!(!p.game.exists());
    }

    #[test]
    fn apply_patch_overlays_and_sets_version_last() {
        let dir = tempfile::tempdir().unwrap();
        let p = Paths::new(dir.path().join("root"));
        game_with_v111(&p);
        let zip = dir.path().join("patch.zip");
        make_zip(&zip, &[("UI/a.png", b"new"), ("UI/b.png", b"b"), (".gitignore", b"*"), ("VERSION", V261)]);
        apply_patch(&p, &zip, "2.6.1").unwrap();
        assert_eq!(std::fs::read(p.game.join("UI/a.png")).unwrap(), b"new");
        assert!(p.game.join("UI/b.png").is_file());
        assert!(!p.game.join(".gitignore").exists());
        assert_eq!(read_version(&p.game.join("VERSION")).unwrap().v, "2.6.1");
        assert!(!p.staging.exists());
    }

    #[test]
    fn apply_patch_unsafe_leaves_game_untouched() {
        let dir = tempfile::tempdir().unwrap();
        let p = Paths::new(dir.path().join("root"));
        game_with_v111(&p);
        let zip = dir.path().join("patch.zip");
        make_zip(&zip, &[("UI/a.png", b"new"), ("../evil", b"e")]);
        assert!(apply_patch(&p, &zip, "2.6.1").is_err());
        assert_eq!(std::fs::read(p.game.join("UI/a.png")).unwrap(), b"old");
        assert_eq!(read_version(&p.game.join("VERSION")).unwrap().v, "1.1.1");
        assert!(!dir.path().join("evil").exists());
    }
}

//! Zip extraction that refuses entries escaping the destination folder.
use anyhow::{bail, Context, Result};
use std::path::{Path, PathBuf};

/// Relative path for a zip entry name, or `None` if it could escape `dest`
/// (absolute, drive letter, `..`, or empty).
fn safe_relative(name: &str) -> Option<PathBuf> {
    if name.starts_with('/') || name.starts_with('\\') || name.contains(':') {
        return None;
    }
    let mut out = PathBuf::new();
    for part in name.split(['/', '\\']) {
        match part {
            "" | "." => continue,
            ".." => return None,
            p => out.push(p),
        }
    }
    if out.as_os_str().is_empty() { None } else { Some(out) }
}

/// Extracts the entries under `prefix` (with `prefix` stripped) into `dest`.
/// Every name is checked before anything is written.
pub fn extract_zip(zip: &Path, dest: &Path, prefix: &str) -> Result<usize> {
    let file = std::fs::File::open(zip).with_context(|| format!("opening {}", zip.display()))?;
    let mut archive = zip::ZipArchive::new(file).with_context(|| format!("{} is not a zip file", zip.display()))?;

    let mut plan = Vec::new();
    for i in 0..archive.len() {
        let entry = archive.by_index(i)?;
        let name = entry.name().to_string();
        if entry.is_dir() || !name.starts_with(prefix) {
            continue;
        }
        let Some(rel) = safe_relative(&name[prefix.len()..]) else {
            bail!("unsafe path in {}: {name}", zip.display());
        };
        plan.push((i, rel));
    }

    for (i, rel) in &plan {
        let target = dest.join(rel);
        if let Some(parent) = target.parent() {
            std::fs::create_dir_all(parent)?;
        }
        let mut entry = archive.by_index(*i)?;
        let mut out = std::fs::File::create(&target).with_context(|| format!("writing {}", target.display()))?;
        std::io::copy(&mut entry, &mut out)?;
    }
    Ok(plan.len())
}

/// Moves every file under `src` into the same place under `dest` (overwriting), then removes `src`.
pub fn move_over(src: &Path, dest: &Path) -> Result<()> {
    fn walk(dir: &Path, base: &Path, dest: &Path) -> Result<()> {
        for entry in std::fs::read_dir(dir)? {
            let path = entry?.path();
            let target = dest.join(path.strip_prefix(base)?);
            if path.is_dir() {
                walk(&path, base, dest)?;
            } else {
                if let Some(parent) = target.parent() {
                    std::fs::create_dir_all(parent)?;
                }
                if target.exists() {
                    std::fs::remove_file(&target)?;
                }
                std::fs::rename(&path, &target).with_context(|| format!("moving into {}", target.display()))?;
            }
        }
        Ok(())
    }
    walk(src, src, dest)?;
    std::fs::remove_dir_all(src)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::testzip::make_zip;

    #[test]
    fn extract_rejects_unsafe() {
        for bad in ["../x", "/abs", "C:/x", r"a\..\..\x"] {
            let dir = tempfile::tempdir().unwrap();
            let zip = dir.path().join("p.zip");
            make_zip(&zip, &[("ok.txt", b"1"), (bad, b"2")]);
            let dest = dir.path().join("out");
            assert!(extract_zip(&zip, &dest, "").is_err(), "accepted {bad}");
            assert!(!dest.exists() || std::fs::read_dir(&dest).unwrap().next().is_none(), "wrote files for {bad}");
        }
    }

    #[test]
    fn extract_strips_prefix() {
        let dir = tempfile::tempdir().unwrap();
        let zip = dir.path().join("a.apk");
        make_zip(&zip, &[("assets/VERSION", b"{}"), ("assets/UI/main.jsc", b"js"), ("classes.dex", b"dex")]);
        let dest = dir.path().join("out");
        assert_eq!(extract_zip(&zip, &dest, "assets/").unwrap(), 2);
        assert!(dest.join("VERSION").is_file());
        assert!(dest.join("UI").join("main.jsc").is_file());
        assert!(!dest.join("classes.dex").exists());
    }

    #[test]
    fn move_over_replaces_and_adds_then_removes_source() {
        let dir = tempfile::tempdir().unwrap();
        let (src, dest) = (dir.path().join("src"), dir.path().join("dest"));
        std::fs::create_dir_all(src.join("UI")).unwrap();
        std::fs::create_dir_all(dest.join("UI")).unwrap();
        std::fs::write(src.join("UI/a.png"), b"new").unwrap();
        std::fs::write(dest.join("UI/a.png"), b"old").unwrap();
        std::fs::write(dest.join("UI/keep.png"), b"k").unwrap();
        move_over(&src, &dest).unwrap();
        assert_eq!(std::fs::read(dest.join("UI/a.png")).unwrap(), b"new");
        assert!(dest.join("UI/keep.png").is_file());
        assert!(!src.exists());
    }
}

//! Brings `game\` up to the version the game server announces, one patch at a time.
use crate::http::Http;
use crate::install::apply_patch;
use crate::paths::Paths;
use crate::urls::{parse_version_reply, patch_url, version_url};
use crate::version::read_version;
use anyhow::{bail, Result};

const MAX_ROUNDS: usize = 10;

#[derive(Debug, Clone, PartialEq)]
pub enum Progress {
    Checking,
    Downloading { done: u64, total: u64 },
    Applying,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Outcome {
    UpToDate(String),
    Updated { from: String, to: String },
}

pub fn run(p: &Paths, http: &dyn Http, on: &mut dyn FnMut(Progress)) -> Result<Outcome> {
    let from = read_version(&p.game.join("VERSION"))?.v;
    for _ in 0..MAX_ROUNDS {
        on(Progress::Checking);
        let vf = read_version(&p.game.join("VERSION"))?;
        let (new, size) = parse_version_reply(&http.get_text(&version_url(&vf))?)?;
        if new == vf.v {
            return Ok(if new == from { Outcome::UpToDate(new) } else { Outcome::Updated { from, to: new } });
        }
        let zip = p.root.join("staging.zip");
        let result = http
            .download(&patch_url(&vf.c, &vf.v, &new), &zip, size, &mut |done| on(Progress::Downloading { done, total: size }))
            .and_then(|_| {
                on(Progress::Applying);
                apply_patch(p, &zip, &new)
            });
        let _ = std::fs::remove_file(&zip);
        result?;
    }
    bail!("the game server kept offering new versions after {MAX_ROUNDS} updates")
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::testzip::make_zip;
    use crate::version::read_version;
    use std::cell::RefCell;

    const V111: &[u8] = br#"{"v":"1.1.1","c":"patch.kageherostudio.com","s":"kageherostudio.com","ch":"108","d":"false"}"#;

    /// Answers the version endpoint via `reply(call_index)`; downloads write `zip_bytes`.
    struct FakeHttp {
        reply: Box<dyn Fn(usize) -> anyhow::Result<String>>,
        zip_bytes: Vec<u8>,
        calls: RefCell<usize>,
        downloads: RefCell<Vec<String>>,
    }

    impl Http for FakeHttp {
        fn get_text(&self, _url: &str) -> anyhow::Result<String> {
            let i = *self.calls.borrow();
            *self.calls.borrow_mut() += 1;
            (self.reply)(i)
        }
        fn download(&self, url: &str, dest: &std::path::Path, expected_len: u64, progress: &mut dyn FnMut(u64)) -> anyhow::Result<()> {
            assert_eq!(expected_len, self.zip_bytes.len() as u64);
            self.downloads.borrow_mut().push(url.to_string());
            std::fs::write(dest, &self.zip_bytes)?;
            progress(expected_len);
            Ok(())
        }
    }

    fn setup() -> (tempfile::TempDir, Paths, Vec<u8>) {
        let dir = tempfile::tempdir().unwrap();
        let p = Paths::new(dir.path().join("root"));
        std::fs::create_dir_all(p.game.join("UI")).unwrap();
        std::fs::write(p.game.join("VERSION"), V111).unwrap();
        std::fs::write(p.game.join("UI/a.png"), b"old").unwrap();
        let zip = dir.path().join("patch.zip");
        make_zip(&zip, &[("UI/a.png", b"new")]);
        let bytes = std::fs::read(&zip).unwrap();
        (dir, p, bytes)
    }

    fn fake(reply: impl Fn(usize) -> anyhow::Result<String> + 'static, zip_bytes: Vec<u8>) -> FakeHttp {
        FakeHttp { reply: Box::new(reply), zip_bytes, calls: RefCell::new(0), downloads: RefCell::new(vec![]) }
    }

    #[test]
    fn up_to_date() {
        let (_d, p, bytes) = setup();
        let http = fake(|_| Ok("1.1.1#1".into()), bytes);
        assert!(matches!(run(&p, &http, &mut |_| {}).unwrap(), Outcome::UpToDate(v) if v == "1.1.1"));
        assert!(http.downloads.borrow().is_empty());
    }

    #[test]
    fn one_patch() {
        let (_d, p, bytes) = setup();
        let len = bytes.len();
        let http = fake(move |_| Ok(format!("2.6.1#{len}")), bytes);
        let out = run(&p, &http, &mut |_| {}).unwrap();
        assert!(matches!(out, Outcome::Updated { ref from, ref to } if from == "1.1.1" && to == "2.6.1"));
        assert_eq!(*http.downloads.borrow(), vec!["https://patch.kageherostudio.com/xxrz/2/1.1.1_2.6.1.zip".to_string()]);
        assert_eq!(std::fs::read(p.game.join("UI/a.png")).unwrap(), b"new");
        assert_eq!(read_version(&p.game.join("VERSION")).unwrap().v, "2.6.1");
        assert!(!p.root.join("staging.zip").exists());
    }

    #[test]
    fn offline() {
        let (_d, p, bytes) = setup();
        let http = fake(|_| Err(anyhow::anyhow!("connection refused")), bytes);
        assert!(run(&p, &http, &mut |_| {}).is_err());
        assert_eq!(std::fs::read(p.game.join("UI/a.png")).unwrap(), b"old");
        assert_eq!(read_version(&p.game.join("VERSION")).unwrap().v, "1.1.1");
    }

    #[test]
    fn garbage_reply() {
        let (_d, p, bytes) = setup();
        let http = fake(|_| Ok("<html>".into()), bytes);
        assert!(run(&p, &http, &mut |_| {}).is_err());
    }

    #[test]
    fn runaway_guard() {
        let (_d, p, bytes) = setup();
        let len = bytes.len();
        let http = fake(move |i| Ok(format!("9.{i}#{len}")), bytes);
        assert!(run(&p, &http, &mut |_| {}).is_err());
        assert_eq!(http.downloads.borrow().len(), 10);
    }
}

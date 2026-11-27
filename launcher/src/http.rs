//! HTTP access, behind a trait so the update flow can be tested without a network.
use crate::urls::redact;
use anyhow::{bail, Context, Result};
use std::io::{Read, Write};
use std::path::Path;
use std::time::Duration;

pub trait Http {
    fn get_text(&self, url: &str) -> Result<String>;
    /// Downloads to `dest`, calling `progress` with bytes received so far.
    /// Fails (leaving no file) unless exactly `expected_len` bytes arrive.
    fn download(&self, url: &str, dest: &Path, expected_len: u64, progress: &mut dyn FnMut(u64)) -> Result<()>;
}

pub struct UreqHttp {
    text: ureq::Agent,
    files: ureq::Agent,
}

impl UreqHttp {
    pub fn new() -> UreqHttp {
        let agent = |read| ureq::AgentBuilder::new().timeout_connect(Duration::from_secs(5)).timeout_read(read).build();
        UreqHttp { text: agent(Duration::from_secs(10)), files: agent(Duration::from_secs(30)) }
    }
}

impl Http for UreqHttp {
    fn get_text(&self, url: &str) -> Result<String> {
        let resp = self.text.get(url).call().with_context(|| format!("requesting {}", redact(url)))?;
        Ok(resp.into_string()?)
    }

    fn download(&self, url: &str, dest: &Path, expected_len: u64, progress: &mut dyn FnMut(u64)) -> Result<()> {
        let part = dest.with_extension(format!("{}.part", dest.extension().and_then(|e| e.to_str()).unwrap_or("")));
        let result = (|| -> Result<u64> {
            let resp = self.files.get(url).call().with_context(|| format!("downloading {}", redact(url)))?;
            let mut reader = resp.into_reader();
            let mut out = std::fs::File::create(&part)?;
            let (mut buf, mut total) = (vec![0u8; 64 * 1024], 0u64);
            loop {
                let n = reader.read(&mut buf)?;
                if n == 0 {
                    break;
                }
                out.write_all(&buf[..n])?;
                total += n as u64;
                progress(total);
            }
            Ok(total)
        })();
        match result {
            Ok(total) if total == expected_len => {
                std::fs::rename(&part, dest)?;
                Ok(())
            }
            Ok(total) => {
                let _ = std::fs::remove_file(&part);
                bail!("download of {} was {total} bytes, expected {expected_len}", redact(url))
            }
            Err(e) => {
                let _ = std::fs::remove_file(&part);
                Err(e)
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Serves `body` once on a free localhost port and returns its URL.
    fn serve_once(body: Vec<u8>) -> String {
        let server = tiny_http::Server::http("127.0.0.1:0").unwrap();
        let url = format!("http://{}/file.zip", server.server_addr().to_ip().unwrap());
        std::thread::spawn(move || {
            if let Ok(req) = server.recv() {
                let _ = req.respond(tiny_http::Response::from_data(body));
            }
        });
        url
    }

    #[test]
    fn download_ok() {
        let dir = tempfile::tempdir().unwrap();
        let dest = dir.path().join("p.zip");
        let mut seen = 0;
        UreqHttp::new().download(&serve_once(vec![7u8; 1000]), &dest, 1000, &mut |n| seen = n).unwrap();
        assert_eq!(std::fs::read(&dest).unwrap().len(), 1000);
        assert!(!dir.path().join("p.zip.part").exists());
        assert_eq!(seen, 1000);
    }

    #[test]
    fn download_size_mismatch() {
        let dir = tempfile::tempdir().unwrap();
        let dest = dir.path().join("p.zip");
        assert!(UreqHttp::new().download(&serve_once(vec![7u8; 999]), &dest, 1000, &mut |_| {}).is_err());
        assert!(!dest.exists());
        assert!(!dir.path().join("p.zip.part").exists());
    }

    #[test]
    fn get_text_unreachable_fails_fast() {
        let start = std::time::Instant::now();
        assert!(UreqHttp::new().get_text("http://127.0.0.1:9/").is_err());
        assert!(start.elapsed() < std::time::Duration::from_secs(6));
    }
}

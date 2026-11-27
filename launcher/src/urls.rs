//! Update endpoints of the game's servers.
use crate::version::VersionFile;
use anyhow::{bail, Result};

pub fn version_url(v: &VersionFile) -> String {
    format!("http://games1.{}:9001/game/version?v={}&ch={}", v.s, v.v, v.ch)
}

pub fn patch_url(cdn: &str, cur: &str, new: &str) -> String {
    let major = new.chars().next().unwrap_or('0');
    format!("https://{cdn}/xxrz/{major}/{cur}_{new}.zip")
}

/// The version endpoint answers `"<new version>#<patch bytes>"`.
pub fn parse_version_reply(body: &str) -> Result<(String, u64)> {
    let Some((ver, size)) = body.trim().split_once('#') else { bail!("unexpected version reply") };
    let size: u64 = size.parse().map_err(|_| anyhow::anyhow!("unexpected version reply"))?;
    if ver.is_empty() || !ver.chars().all(|c| c.is_ascii_digit() || c == '.') {
        bail!("unexpected version reply");
    }
    Ok((ver.to_string(), size))
}

/// URL without its query string, safe for logs (queries can carry credentials).
pub fn redact(url: &str) -> &str {
    url.split('?').next().unwrap_or(url)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::version::VersionFile;

    fn vf() -> VersionFile {
        VersionFile { v: "1.1.1".into(), c: "patch.kageherostudio.com".into(), s: "kageherostudio.com".into(), ch: "108".into(), d: "false".into() }
    }

    #[test]
    fn version_url_uses_games1_server_version_and_channel() {
        assert_eq!(version_url(&vf()), "http://games1.kageherostudio.com:9001/game/version?v=1.1.1&ch=108");
    }

    #[test]
    fn patch_url_uses_first_char_of_new_version() {
        assert_eq!(patch_url("patch.kageherostudio.com", "1.1.1", "2.6.1"), "https://patch.kageherostudio.com/xxrz/2/1.1.1_2.6.1.zip");
    }

    #[test]
    fn parse_version_reply_reads_version_and_size() {
        assert_eq!(parse_version_reply("2.6.1#281207445\n").unwrap(), ("2.6.1".to_string(), 281207445));
    }

    #[test]
    fn parse_version_reply_rejects_garbage() {
        assert!(parse_version_reply("<html>").is_err());
        assert!(parse_version_reply("2.6.1#").is_err());
    }

    #[test]
    fn redact_drops_query_string() {
        assert_eq!(redact("http://a/game/login?acc=me@x.com&pwd=1"), "http://a/game/login");
        assert_eq!(redact("http://a/game/servers"), "http://a/game/servers");
    }
}

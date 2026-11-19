# Public alpha release checks

Baseline: Alpha v.37, built commit `8af683d0926ce27b1afd530d72850d119bb596f1`. Publishing the repository was authorized on 18 November 2026 after the source-visibility implications were explained.

- The repository includes source and history. Each binary release includes corresponding source archives and component notices.
- All 19 release attachments passed download/read-back checksum verification. The downloaded game-folder ZIP passed CRC and 567-file manifest comparison.
- The source and asset archives exclude downloaded/custom songs, user profiles, scores, runtime logs, console configuration and credentials. The approved 30 original PC bundled songs are included.
- The tracked-file audit found no obvious private keys, GitHub tokens, private network addresses or runtime userdata. A targeted history scan found no corresponding matches in project code or documentation.
- Redistribution permission for extracted artwork, fonts and sounds was recorded as “Have permission to redistribute.” The original PC bundled songs were expressly requested for the distribution. The existing asset notices record that attestation and retain each asset's rights; no independent licence grant or standalone asset licence is asserted here.
- The embedded notices describe a tester build, which remains accurate for a public alpha. References to private access in archived installation/source documents describe the original packaging stage. Published archives and hashes remain unchanged.
- Physical PKG installation, current exFAT mounting, a five-lane drum kit and four physical instruments remain open checks, disclosed in KNOWN_ISSUES.md and release notes. No hardware result is inferred from bot runs.

Full updates receive a new version/tag and matching downloads. Documentation changes do not replace released binaries.

# Upstream investigation

## Native PS5 package acceptance

Owner: current Encore integration work. No contribution has been submitted.

The native package investigation followed these upstreams:

- [SvenGDK/LibProsperoPKG](https://github.com/SvenGDK/LibProsperoPKG), `main`,
  `748eabf1b7d17819528cabf367d8e27109d8fce3`.
- [drakmor/LibProsperoPkg](https://github.com/drakmor/LibProsperoPkg),
  `feature/full-ppr-pkg-roundtrip`,
  `5e26c8ae65f4b3cf88dcc7c84226ea9718c38e5e`.
- [OnTheFly5/LibProsperoPKG](https://github.com/OnTheFly5/LibProsperoPKG),
  streaming work at `36cc8807006597cf87a578ef5a114e512ad36e1e`; its documentation
  does not establish console compatibility.
- [PS5 UltraPack](https://github.com/knutwurst/ps5-ultrapack), which documents
  hardware checks and a PS5 application-volume DRM type correction.

Checked on 2026-11-19: original upstream current code and all available issues
(2) and PRs (0); the closed SHA3-runtime issue and open extraction-overflow issue
are unrelated. The original upstream is archived. An attempted focused issue
report was refused by GitHub as read-only; it was not published. The maintained
fork disables issues and has no listed open/closed PRs. Existing contribution
requirements and source/branch ownership must be refreshed before submission.

Local diagnosis is in [native PKG validation](docs/native-pkg-validation.md).
The image-mount failure is not yet isolated to a complete, proved library fix,
so no speculative PR has been opened. A separate host issue was found in the
legacy nested-image writer: `FSFile(PfsBuilder)` leaves the stream cursor inside
the last metadata block after `WriteImage`, conflicting with the strict file
extent check. Restoring its cursor to the planned end permits the host build,
but the resulting legacy image still fails console installation. A standalone
regression and complete upstream change remain pending; this is not a native
PKG launch fix.

### Resolution used by Encore

The released Linux `ffpfsc-pkg-tool` from PS5 UltraPack 2.6.1 successfully installs
and launches the same homebrew payload on the existing console setup. Full PKG
embedded-game rendering and automatic update staging/restart also pass. Encore
pins that tool by SHA-256 and supplies its own metadata module to avoid default
module injection. This reuses existing packaging work; it does not claim the
rejected candidate writers were fixed locally. No issue or PR was published.
The unused legacy cursor issue remains a separate host-only candidate.

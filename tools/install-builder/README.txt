CH: Encore — Personal Install Builder

Create a complete PS5 install on your own PC. Public downloads contain only
this builder and the asset-free game template. Your PC assets are added locally.

Windows 10/11 x64
1. Install Python 3.10 or newer from python.org (if needed).
2. Extract this entire builder ZIP and double-click Start-Builder.cmd.
3. Select a detected PC installation, or Browse to a custom/portable location.
4. Choose the output folder and Game folder, exFAT image, or native PKG/fPKG.
5. Confirm permission to use your local resources, then select Build install.

The launcher installs pinned Python dependencies in a local .venv. On first
exFAT/PKG use the builder downloads and checksum-verifies the packaging backend
from its official source/release. No additional console file transfer is needed.
It does not install another desktop app or require administrator access or WSL.

Use the resulting title folder or container with your compatible PS5 homebrew
setup. These are native homebrew/debug PKGs, not retail-signed applications.
First launch prepares the included personal assets automatically, with a
progress display. Songs, profiles and scores are preserved. Songs are not
imported from the PC game. Future code updates preserve your imported assets.

Detection checks common standalone and launcher gameFiles/version locations;
custom installations may need Browse. Detection is bounded, not a full-disk
search. Each asset source is verified before extraction. Only the fingerprinted
PC resource version in asset-importer/recipes.json is supported; detection of a
folder does not imply that its installed version is supported.

The PC installation is read only. Failed/cancelled builds stay temporary; an
existing output is never intentionally overwritten. Choose a new output folder
for each repeat build. Keep the finished personal install private: it contains
your imported resources. Do not redistribute it, upload it to GitHub, or attach
it to issue reports. Only the original public builder ZIP is shareable.

Linux/macOS: run Start-Builder.sh; Python needs venv and Tk support. Folder and
exFAT creation use platform-independent Python. Native PKG creation currently
supports Windows/Linux x64. Windows outputs are tested separately from the
native PS5 game; see release testing notes for actual hardware coverage.

Command line (after installing requirements.txt):
  python builder.py --detect
  python builder.py --game "C:\Games\your PC game" --output "D:\My Builds" --format exfat --authorized
Formats: folder, exfat, pkg. --pkg-tool accepts the exact pinned native package
writer instead of downloading it. --cache can select a different tool cache.

Packaging dependencies are downloaded, not bundled or inserted into the game:
PS5 UltraPack 2.6.1, GPL-3.0 and component licences/notices; its unmodified
MkPFS exFAT writer is invoked in a separate process, and its standalone native
package writer is used for PKG builds. Original release/source archives are
retained in the local tool cache with all licences and source references.
https://github.com/knutwurst/ps5-ultrapack/releases/tag/v2.6.1

This does not grant permission to extract/reuse the PC resources or resolve
copyright, licence or anti-circumvention issues. No protected-song decryption
or access-control bypass is included. See RIGHTS_AND_DISTRIBUTION.md.

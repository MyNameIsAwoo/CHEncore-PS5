CH: Encore 1.5.2 — Standalone Windows Install Builder

Windows users: download CH_Encore_1.5.2_Install_Builder.exe and double-click it.
It is one self-contained file: Python 3.12, importer dependencies, the asset-free
game template and packaging tools are already included. No ZIP extraction,
Python installation, compiler, administrator access or additional download is
needed to create a folder, exFAT image or native homebrew PKG on Windows x64.

1. Select a detected PC installation or Browse to your custom/portable folder.
2. Choose an output folder and Game folder, exFAT image or Native PKG / fPKG.
3. Confirm permission to use your resources, then select Build install.
4. Install the completed output using your compatible PS5 homebrew setup.

Your personal assets are added locally. First launch on PS5 prepares them
automatically; no separate loose file, FTP transfer or import selection is
required. Songs, profiles and scores are preserved. No songs are included.
These are native homebrew/debug PKGs, not retail-signed applications. The new
bundled package backend needs a fresh physical PS5 installation test.

The public executable contains no original PC artwork, fonts, sounds or songs.
It reads only your local fingerprinted resources. Unsupported versions fail
before extraction. The original installation stays unchanged. No protected
song decryption, executable analysis, keys or access-control bypass is used.
Keep completed personal installs private: they contain your imported resources.
Do not upload them to GitHub or distribute them. Only share the public builder.

The executable uses Packwright 1.3.0 as a separate packaging process. Its full
corresponding source, GPL-3.0 licence and third-party notices are embedded, along
with desktop dependency licence notices. No tool modules are injected into the
game. Source revision: 7039684af5c978ae69ee619d44e98102a61104a6
https://github.com/itsmemac/Packwright

Developer/source fallback (not required by the Windows executable):
- Windows source launcher: Start-Builder.cmd explicitly selects Python 3.12 x64
  and uses .venv-py312, bypassing a failed .venv created with Python 3.15.
- Linux/macOS: Start-Builder.sh requires Python 3.11+, Tk and venv support and
  installs requirements.txt. Its pinned source backends download on first use.
- Freezing an executable requires native Windows CPython 3.12 x64 and
  PyInstaller 6.16.0; see ps5/tools/build-windows-builder.py.

No permission to extract/reuse original resources or legal clearance is granted
by this tool. Read RIGHTS_AND_DISTRIBUTION.md.

# User asset setup

CH: Encore v1.5.0 ships without original PC artwork, fonts, sound effects or songs. Its setup screen works without them.

Use only resources you have permission to extract and use. Owning or downloading a copy of the PC game does not by itself establish that permission. The importer is not a way to bypass the original developer’s licence restrictions. Read [rights and distribution notes](RIGHTS_AND_DISTRIBUTION.md).

## On the PC

Download and extract `CH_Encore_1.5.0_asset-importer.zip`. Install Python 3.10 or newer. Open a terminal in the importer folder:

```powershell
py -m venv .venv
.venv\Scripts\python.exe -m pip install -r requirements.txt
.venv\Scripts\python.exe create_asset_pack.py "C:\path\to\your\PC game"
```

On Linux/macOS, use `python3 -m venv .venv`, `.venv/bin/python -m pip install -r requirements.txt`, then `.venv/bin/python create_asset_pack.py /path/to/your/game`.

Select the complete installation or its original `*_Data` directory. The importer reads only its fingerprinted plain asset files and explicit interface sound files. It never executes the PC game, reads decompiled scripts, accepts decryption keys, or reads/decrypts its protected song bundles. Unsupported versions fail before extraction. The source installation remains unchanged.

It creates `EncoreAssets-v1.encore-assets` outside the PC installation. The generated pack contains your original resources: keep it private and do not attach it to project issues or redistribute it with a game build.

## On PS5

Copy that file to the root of an **exFAT** USB drive. Connect it and open the game. With a DualSense, press Cross to import; Circle cancels. The importer detects `/mnt/usb0` through `/mnt/usb7`. As an alternative, upload the pack to `/data/homebrew/EncoreAssets-v1.encore-assets` using your existing setup.

The game validates the exact required file list and every SHA-256 digest, writes a separate generation, and activates it only after the import completes. Cancelling, unplugging or a damaged pack cannot replace an existing working pack. Asset data stays in `/data/homebrew/PPSA99200/userdata/pc-assets/`, separate from your songs, saves and executable.

Normal code updates preserve this directory. If a future version changes the required asset layout, setup asks for a matching new pack. It never obtains copyrighted PC assets from GitHub or another download service.

## Songs

No songs are imported from the PC game. Add only charts and audio you are authorized to use, through the existing browser or SONGS storage. The old protected bundled-song conversion tool is not included.

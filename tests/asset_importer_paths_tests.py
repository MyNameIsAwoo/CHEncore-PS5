# Copyright (C) 2026 HOWL
# SPDX-License-Identifier: MIT
import importlib.util,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('importer',root/'tools/asset-importer/create_asset_pack.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
with tempfile.TemporaryDirectory() as temporary:
 root=Path(temporary);game=root/'Game';data=game/'Game_Data';data.mkdir(parents=True)
 for output in [game/'new'/'assets.encore-assets',data/'new'/'assets.encore-assets',root/'new'/'game.exe']:
  try:module.prepare_output(output,data)
  except ValueError:pass
  else:raise AssertionError('Unsafe output accepted')
  assert not output.parent.exists() or output.parent==game or output.parent==data
 output=root/'out'/'assets.encore-assets';assert module.prepare_output(output,data)==output and output.parent.is_dir() and not output.exists()
 assert list(game.iterdir())==[data] and not list(data.iterdir())
print('PASS importer output validation: PC installation remains unchanged, unsafe outputs create no directories')

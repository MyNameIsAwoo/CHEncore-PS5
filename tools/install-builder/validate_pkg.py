# Copyright (C) 2026 HOWL
# SPDX-License-Identifier: MIT
"""Preserve the writer's structured validation result for the parent builder."""
import json
from pathlib import Path
import subprocess
import sys
if __name__ == '__main__':
    result = subprocess.run([sys.argv[1], 'validate', sys.argv[2], '--json'], check=True, capture_output=True, text=True)
    data = json.loads(result.stdout)
    if not isinstance(data.get('fail'), int) or data['fail']:
        raise ValueError('Native package validation failed.')
    Path(sys.argv[3]).write_text(json.dumps(data), encoding='utf-8')
    print('Native PKG validation passed.', flush=True)

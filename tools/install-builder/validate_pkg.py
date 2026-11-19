# Copyright (C) 2026 HOWL
# SPDX-License-Identifier: MIT
"""Preserve the writer's structured validation result for the parent builder."""
import json
from pathlib import Path
import subprocess
import sys
def main(arguments=None):
    tool, package, output = sys.argv[1:] if arguments is None else arguments
    result = subprocess.run([tool, 'validate', package, '--json'], check=True, capture_output=True, text=True)
    data = json.loads(result.stdout)
    if not isinstance(data.get('fail'), int) or data['fail']:
        raise ValueError('Native package validation failed.')
    Path(output).write_text(json.dumps(data), encoding='utf-8')
    print('Native PKG validation passed.', flush=True)

if __name__ == '__main__': main()

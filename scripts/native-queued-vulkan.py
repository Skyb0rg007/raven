#!/usr/bin/env python3
"""Require correct GPU results and actual queued dispatch through the production loader."""

import os
import re
import subprocess
import sys

runtime, fixture = sys.argv[1:]
result = subprocess.run([runtime, '--run-process', '--execution=native', fixture],
                        env=os.environ | {'RAVEN_PARALLEL': '1', 'RAVEN_QUEUED_GRAPHICS': '1', 'RAVEN_PROFILE': '1'},
                        capture_output=True, text=True, timeout=20)
output = result.stdout + result.stderr
queued = sum(int(value) for value in re.findall(r'profile: queued calls=(\d+)', output))
if result.returncode or 'Native entry returned 0 ' not in output or not queued or 'Validation Error:' in output or 'VUID-' in output:
    raise SystemExit(f'queued Vulkan check failed: exit={result.returncode}, queued={queued}\n{output}')
print(f'GPU output verified through {queued} queued calls')

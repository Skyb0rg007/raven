#!/usr/bin/env python3
"""Check shared-wait deadlines and private-only deadlock detection."""

import os
from pathlib import Path
import subprocess
import sys
import time


def main():
    raven, client = (str(Path(value).resolve()) for value in sys.argv[1:3])
    env = os.environ | {"RAVEN_TRACE_DEBUG": "1"}
    for shared in (False, True):
        started = time.monotonic()
        result = subprocess.run(
            [raven, "--run-process", "--execution=native", "--timeout-ms=1500", client,
             "--", "--never-shared" if shared else "--never"],
            env=env, capture_output=True, text=True, timeout=6)
        elapsed = time.monotonic() - started
        output = result.stdout + result.stderr
        entered = "Shared wait entered" if shared else "Private wait entered"
        expected = "native entry timed out" if shared else "every thread waits without a timeout"
        if not result.returncode or entered not in output or expected not in output:
            raise RuntimeError(f"unexpected wait result ({result.returncode}): {output}")
        if shared and ("deadlocked" in output or elapsed < 1.2):
            raise RuntimeError("shared wait ended before the runtime deadline")
    print("Shared-wait deadline and private deadlock guard verified")


if __name__ == "__main__":
    main()

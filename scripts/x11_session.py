"""An isolated X server with a window manager for desktop tests, and captures of what it shows.

Xvfb keeps its screen in an XWD file under the work directory (-fbdir), which capture() converts to PNG, so no capture tool is needed.
Openbox supplies the EWMH behaviour a desktop session has: maximization, interactive moves and focus. It centres new windows on the
screen, as labwc does for the Wayland tests, so a workflow finds a program's dialogs in the same place on both desktops.
"""

from pathlib import Path
import os
import shutil
import struct
import subprocess
import time

from PIL import Image

MANAGER_CONFIGURATION = """<?xml version="1.0"?>
<openbox_config xmlns="http://openbox.org/3.4/rc">
  <applications>
    <application class="*" type="normal">
      <position force="yes"><x>center</x><y>center</y></position>
    </application>
    <application class="*" type="dialog">
      <position force="yes"><x>center</x><y>center</y></position>
    </application>
  </applications>
</openbox_config>
"""


class X11Session:
    def __init__(self, work, size=(1280, 720), manager=True):
        self.work, self.size, self.manager = Path(work), size, manager
        self.server = self.window_manager = None
        self.display = None

    def environment(self):
        return {"DISPLAY": f":{self.display}"}

    def __enter__(self):
        self.work.mkdir(parents=True, exist_ok=True)
        for number in range(90, 190):
            if not Path(f"/tmp/.X11-unix/X{number}").exists() and not Path(f"/tmp/.X{number}-lock").exists():
                self.display = number
                break
        else:
            raise RuntimeError("no free X display number")
        log = (self.work / "xvfb.log").open("wb")
        self.server = subprocess.Popen(["Xvfb", f":{self.display}", "-screen", "0", f"{self.size[0]}x{self.size[1]}x24", "-fbdir",
                                        str(self.work), "-nolisten", "tcp", "-noreset"], stdout=log, stderr=log)
        deadline = time.monotonic() + 10
        while not Path(f"/tmp/.X11-unix/X{self.display}").exists():
            if self.server.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError("isolated X server did not start")
            time.sleep(0.05)
        env = os.environ | self.environment()
        # Xvfb draws the pointer into the screen it saves; it starts in the middle, where windows are placed.
        subprocess.run(["xdotool", "mousemove", str(self.size[0] - 1), str(self.size[1] - 1)], env=env, check=True, timeout=10)
        if self.manager:
            manager_log = (self.work / "openbox.log").open("wb")
            configuration = self.work / "openbox.xml"
            configuration.write_text(MANAGER_CONFIGURATION)
            self.window_manager = subprocess.Popen(["openbox", "--sm-disable", "--config-file", str(configuration)], env=env,
                                                   stdout=manager_log, stderr=manager_log)
            deadline = time.monotonic() + 10
            # The desktop count is an EWMH property the window manager publishes once it manages the screen.
            while subprocess.run(["xdotool", "get_num_desktops"], env=env, capture_output=True).returncode:
                if self.window_manager.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError("window manager did not start")
                time.sleep(0.1)
        return self

    def capture(self, path):
        """Converts the server's XWD screen file to an image at path and returns it."""
        data = (self.work / "Xvfb_screen0").read_bytes()
        header = struct.unpack(">25I", data[:100])
        header_size, width, height, byte_order, bits, stride, colors = header[0], header[4], header[5], header[7], header[11], header[12], header[19]
        if bits != 32 or byte_order != 0:
            raise RuntimeError("unexpected X server screen format")
        offset = header_size + colors * 12
        image = Image.frombuffer("RGB", (width, height), data[offset:offset + stride * height], "raw", "BGRX", stride, 1)
        image.save(path)
        return image

    def __exit__(self, *details):
        for process in (self.window_manager, self.server):
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=10)
        screen = self.work / "Xvfb_screen0"
        if screen.exists():
            screen.unlink()
        return False


def available():
    return all(shutil.which(tool) for tool in ("Xvfb", "openbox", "xdotool"))

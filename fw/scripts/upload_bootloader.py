import os

from SCons.Script import DefaultEnvironment


env = DefaultEnvironment()
board = env.BoardConfig()

upload_port = env.subst("$UPLOAD_PORT")
uploader = env.subst("$UPLOADER")
mcu = board.get("build.mcu", "atmega1284p")

boot = board.get("bootloader", {})
boot_file = boot.get("file")
lfuse = boot.get("lfuse", "0xF7")
hfuse = boot.get("hfuse", "0xD6")
efuse = boot.get("efuse", "0xFF")
unlock_bits = boot.get("unlock_bits", "0xFF")
lock_bits = boot.get("lock_bits", "0xCF")

framework_dir = env.PioPlatform().get_package_dir("framework-arduino-avr-mightycore")
if not framework_dir:
    print("MightyCore package was not found. Run `pio pkg install` first.")
    env.Exit(1)

bootloader_path = os.path.join(framework_dir, "bootloaders", boot_file)
if not os.path.isfile(bootloader_path):
    print("Bootloader image not found:", bootloader_path)
    env.Exit(1)

cmd = [
    uploader,
    "-p",
    mcu,
    "-c",
    "stk500v2",
    "-P",
    upload_port,
    "-e",
    "-U",
    f"lock:w:{unlock_bits}:m",
    "-U",
    f"efuse:w:{efuse}:m",
    "-U",
    f"hfuse:w:{hfuse}:m",
    "-U",
    f"lfuse:w:{lfuse}:m",
    "-U",
    f"flash:w:{bootloader_path}:i",
    "-U",
    f"lock:w:{lock_bits}:m",
]

env.Replace(UPLOADCMD=cmd)

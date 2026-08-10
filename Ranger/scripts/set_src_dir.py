#
# Point PROJECT_SRC_DIR at a per-environment source folder.
#
# PlatformIO treats src_dir as a global [platformio] option, so it cannot be set
# per [env:...] in platformio.ini. This pre-script reads a custom_src_dir option
# from the active environment and overrides the source directory for that build.
#

import os

Import("env")

custom_src_dir = env.GetProjectOption("custom_src_dir", default=None)

if custom_src_dir:
    env["PROJECT_SRC_DIR"] = os.path.join(env["PROJECT_DIR"], custom_src_dir)
    print("Using source directory: %s" % env["PROJECT_SRC_DIR"])

#!/usr/bin/env python3
# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import re

config = (Path(__file__).resolve().parent.parent / "src/akyuu/config.h").read_text()
parts = [re.search(r"^#define AKYUU_VERSION_" + name + r" +(\d+)$", config, re.M)[1]
         for name in ("MAJOR", "MINOR", "PATCH")]
pre = re.search(r'^#define AKYUU_VERSION_PRE +"([^\"]*)"$', config, re.M)[1]
print(".".join(parts) + ("-" + pre if pre else ""))

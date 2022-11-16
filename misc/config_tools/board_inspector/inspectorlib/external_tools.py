# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import logging
import subprocess # nosec

available_tools = {}

class ExecutableNotFound(ValueError):
    pass

def run(command, **kwargs):
    """Run the given command which can be either a string or a list."""
    try:
        if isinstance(command, list):
            full_path = available_tools[command[0]]
            kwargs.setdefault("capture_output", True)
            return subprocess.run([full_path] + command[1:], **kwargs)
        elif isinstance(command, str):
            parts = command.split(" ", maxsplit=1)
            full_path = available_tools[parts[0]]
            kwargs["shell"] = True
            kwargs.setdefault("stdout", subprocess.PIPE)
            kwargs.setdefault("stderr", subprocess.PIPE)
            kwargs.setdefault("close_fds", True)
            cmd = f"{full_path} {parts[1]}" if len(parts) == 2 else full_path
            return subprocess.Popen(cmd, **kwargs)
    except KeyError:
        raise ExecutableNotFound

    assert False, f"A command is expected either a list or a string: {command}"

def detect_tool(name):
    """Detect the full path of a system tool."""
    system_paths = [
        "/usr/bin/",
        "/usr/sbin/",
        "/usr/local/bin/",
        "/usr/local/sbin/",
    ]

    # Look for `which` first.
    for path in system_paths:
        candidate = os.path.join(path, "which")
        if os.path.exists(candidate):
            available_tools["which"] = candidate
            break

    try:
        result = run(["which", "-a", name])
        for path in result.stdout.decode().strip().split("\n"):
            if any(map(lambda x: path.startswith(x), system_paths)):
                logging.debug(f"Use {name} found at {path}.")
                return path
    except ExecutableNotFound:
        pass

    logging.critical(f"'{name}' cannot be found. Please install it and run again.")
    return None

def locate_tools(tool_list):
    """Find a list of tools under common system executable paths. Return True if and only if all tools are found."""
    had_error = False

    for tool in tool_list:
        full_path = detect_tool(tool)
        if full_path != None:
            available_tools[tool] = full_path
        else:
            had_error = True

    return not had_error

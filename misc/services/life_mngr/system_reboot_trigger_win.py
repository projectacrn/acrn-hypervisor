#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#
import socket
import sys

if __name__ == "__main__":
    HOST = '127.0.0.1'
    PORT = 8193
    SYS_REBOOT_REQ = 'req_sys_reboot'
    MSG_LEN = 32

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    print(["Socket Created"])

    try:
        s.connect((HOST,PORT))
        print("[Connection established]")
    except Exception:
        print('[Connection error:  ' + HOST + ":" + str(PORT)+']')
        s.close()

    try:
        s.send(SYS_REBOOT_REQ.encode('utf-8'))
    except Exception as _:
        raise _
    print(["System reboot request sent\n"])

    try:
        data_input = (s.recv(MSG_LEN).decode("UTF-8"))
    except Exception:
        pass
    print("Waiting for ACK message...: ", data_input)
    s.close()

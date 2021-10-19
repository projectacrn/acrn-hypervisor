#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#
import socket
import sys

if __name__ == "__main__":
    HOST = '127.0.0.1'
    PORT = 8193
    SHUTDOWN_REQ = 'req_sys_shutdown'
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
        s.send(SHUTDOWN_REQ.encode('utf-8'))
    except Exception as _:
        raise _
    print(["Shutdown request sent\n"])

    try:
        data_input = (s.recv(MSG_LEN).decode("UTF-8"))
    except Exception:
        pass
    print("Waiting for ACK message...: ", data_input)
    s.close()
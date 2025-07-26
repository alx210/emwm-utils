/*
 * Copyright (C) 2018-2025 alx@fastestcode.org
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Session manager IPC globals
 */

#define XMSM_ATOM_NAME "_XM_SESSION_MANAGER"
#define XMSM_PID_ATOM_NAME "_XM_SESSION_MANAGER_PID"
#define XMSM_CMD_ATOM_NAME "_XM_SESSION_MANAGER_CMD"
#define XMSM_CFG_ATOM_NAME "_XM_SESSION_MANAGER_CFG"

/* No larger than client message event's data field */
#define XMSM_CMDLEN_MAX 20

#define XMSM_LOGOUT_CMD "LOGOUT"
#define XMSM_LOCK_CMD "LOCK"
#define XMSM_SUSPEND_CMD "SUSPEND"

#define XMSM_CFG_SUSPEND 0x0001
#define XMSM_CFG_LOCK    0x0002

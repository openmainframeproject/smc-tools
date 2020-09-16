/*
 * SMC Tools - Shared Memory Communication Tools
 *
 * Copyright IBM Corp. 2016, 2018
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <gnu/lib-names.h>
#include <netdb.h>
#include <dlfcn.h>
#include <errno.h>
#include <search.h>

#define DLOPEN_FLAG RTLD_LAZY

#ifndef AF_SMC
#define AF_SMC 43
#endif

#ifndef SMCPROTO_SMC
#define SMCPROTO_SMC		0	/* SMC protocol, IPv4 */
#define SMCPROTO_SMC6		1	/* SMC protocol, IPv6 */
#endif

int (*orig_socket)(int domain, int type, int protocol);
static void *dl_handle = NULL;

static void initialize(void);

static int debug_mode = 0;

#define GET_FUNC(x) \
if (dl_handle) { \
	char *err; \
	dlerror(); \
	orig_ ## x=dlsym(dl_handle,#x); \
	if ((!orig_ ## x)&&(err=dlerror())) { \
		fprintf(stderr, "dlsym failed on " #x ": %s\n",err); \
		orig_ ## x=&emergency_ ## x; \
	} \
} else { \
	orig_ ## x=&emergency_ ## x; \
}

static void dbg_msg(FILE *f, const char *format, ...)
{
	va_list vl;

	if (debug_mode) {
		va_start(vl, format);
		vfprintf(f, format, vl);
		va_end(vl);
	}
}

static int emergency_socket(int domain, int type, int protocol)
{
	errno = EINVAL;
	return -1;
}

int socket(int domain, int type, int protocol)
{
	int rc;

	if (!dl_handle)
		initialize();

	/* check if socket is eligible for AF_SMC */
	if ((domain == AF_INET || domain == AF_INET6) &&
	    // see kernel code, include/linux/net.h, SOCK_TYPE_MASK
	    (type & 0xf) == SOCK_STREAM &&
	    (protocol == IPPROTO_IP || protocol == IPPROTO_TCP)) {
		dbg_msg(stderr, "libsmc-preload: map sock to AF_SMC\n");
		if (domain == AF_INET)
			protocol = SMCPROTO_SMC;
		else /* AF_INET6 */
			protocol = SMCPROTO_SMC6;

		domain = AF_SMC;
	}

	rc = (*orig_socket)(domain, type, protocol);

	return rc;
}

static void set_debug_mode(const char *var_name)
{
	char *var_value;

	var_value = getenv(var_name);
	debug_mode = 0;
	if (var_value != NULL)
		debug_mode = (var_value[0] != '0');
}

static void initialize(void)
{
	set_debug_mode("SMC_DEBUG");

	dl_handle = dlopen(LIBC_SO, DLOPEN_FLAG);
	if (!dl_handle)
		dbg_msg(stderr, "dlopen failed: %s\n", dlerror());
	GET_FUNC(socket);
}

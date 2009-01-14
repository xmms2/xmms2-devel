/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */


/** @file
 * Simple launcher that spawns off xmms2d in the background.
 * The launcher exits when xmms2d's ipc is up.
 */

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "xmms_configuration.h"
#include "xmmsc/xmmsc_util.h"

#include <glib.h>
#include <glib/gstdio.h>

static char startup_msg[] = "\n--- Starting new xmms2d ---\n";

int
main (int argc, char **argv)
{
	pid_t pid;
	int i, fd, max_fd;
	int pipefd[2];
	const gchar *logfile = NULL;
	const gchar *pidfile = NULL;
	GError *error = NULL;
	GOptionContext* context = NULL;
	GOptionEntry opts[] = {
		{"logfile", 'l', 0, G_OPTION_ARG_FILENAME, &logfile, "Redirect logs to <file>", "<file>"},
		{"pidfile", 'P', 0, G_OPTION_ARG_FILENAME, &pidfile, "Save xmms2d pid in <file>", "<file>"},
		{NULL}
	};

	context = g_option_context_new ("- XMMS2 launcher");
	g_option_context_set_ignore_unknown_options (context, TRUE);
	g_option_context_add_main_entries (context, opts, NULL);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_print ("xmms2-launcher: %s\n", error->message);
		g_clear_error (&error);
	}
	g_option_context_free (context);

	if (pipe (&pipefd[0]) == -1) {
		perror ("pipe");
		exit (1);
	}
	
	if (!logfile) {
		char cache[PATH_MAX];
		xmms_usercachedir_get (cache, PATH_MAX);
		logfile = g_build_filename (cache, "xmms2d.log", NULL);
		if (!g_file_test (cache, G_FILE_TEST_IS_DIR)) {
			g_mkdir_with_parents (cache, 0755);
		}
	}
	g_print ("Log output will be stored in %s\n", logfile);

	pid = fork ();
	if (pid) {
		char t;
		int res;
		close (pipefd[1]);

		for (;;) {
			res = read (pipefd[0], &t, 1);
			if (res != -1 || errno == EINTR)
				break;
		}
		if (res == -1)
			perror ("read");
		if (res == 0) {
			printf ("startup failed!\n");
			exit (1);
		}
		printf("xmms2 started\n");
		exit(0);
	}

	/* Change stdin/out/err */
	fd = open ("/dev/null", O_RDONLY);
	dup2 (fd, STDIN_FILENO);

	fd = open (logfile, O_WRONLY|O_CREAT|O_APPEND, 0644);
	write(fd, startup_msg, sizeof (startup_msg) - 1);
	dup2 (fd, STDOUT_FILENO);
	dup2 (fd, STDERR_FILENO);

	/* Close all unused file descriptors */
	max_fd = sysconf (_SC_OPEN_MAX);
	for (i = 3; i <= max_fd; i++) {
		if (pipefd[1] != i)
			close (i);
	}

	/* make us process group leader */
	setsid ();

	/* fork again to reparent to init */
	pid = fork();
	if (pid && pidfile) {
		FILE *pfile = NULL;
		pfile = g_fopen (pidfile, "w");
		if (pfile) {
			g_fprintf (pfile, "%d", (int) pid);
		}
	}
	if (!pid) {
		char *args[32];
		char buf[32];
		int i;
		args[0] = BINDIR "/xmms2d";
		snprintf (buf, 32, "--status-fd=%d", pipefd[1]);
		args[1] = buf;
		for (i = 1; i < argc && i < 30; i++) {
			args[i + 1] = argv[i];
		}
		args[i + 1] = NULL;

		execvp (args[0], args);
		exit (1);
	}
	exit (0);
}

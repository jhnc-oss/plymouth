/* test-server-connection-hangup.c
 *
 * Regression test for issues #125 (use-after-free on client kill) and #126
 * (password prompt not dismissed when non-interactive unlock completes).
 *
 * The fix adds a connection_hangup_handler callback to ply_boot_server_t that
 * fires *before* the connection reference is dropped, giving the caller a
 * window to cancel pending prompts while the connection pointer is still valid.
 *
 * This test requires root because plymouthd verifies SO_PEERCRED credentials
 * before processing password requests.  When not running as root it reports
 * every case as a TAP skip rather than failing.
 *
 * Two scenarios are verified:
 *
 *  Test 1 – hangup callback fires with valid connection pointer
 *    A client sends a password request then disconnects without answering.
 *    The test asserts that connection_hangup_handler is called with the same
 *    connection pointer that ask_for_password_handler received, and that the
 *    pending trigger can be safely pulled from within the callback (proving
 *    the connection is still live when the handler fires).
 *
 *  Test 2 – server remains responsive after first client disconnects
 *    After the first client disconnects and the pending prompt is cancelled,
 *    a second client successfully gets its password request queued.  This
 *    directly guards against the "prompt remains on screen forever" regression
 *    described in issue #126.
 */

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ply-boot-protocol.h"
#include "ply-boot-server.h"
#include "ply-event-loop.h"
#include "ply-test.h"
#include "ply-trigger.h"

#define TEST_TIMEOUT_SECONDS 10

/* -------------------------------------------------------------------------
 * Shared test state
 *
 * The scenario is driven once by main(), which then hands the collected
 * observations to the test cases below.  State lives at file scope so the
 * cases can inspect it without threading a context through ply_test_run().
 * ---------------------------------------------------------------------- */

typedef struct
{
        bool                   ask1_called;
        bool                   ask2_called;
        bool                   hangup1_called;
        bool                   client_exited_cleanly;
        ply_boot_connection_t *ask1_connection;
        ply_boot_connection_t *hangup1_connection;
        ply_trigger_t         *ask1_trigger;
        ply_event_loop_t      *loop;
} test_state_t;

static test_state_t test_state;

/* -------------------------------------------------------------------------
 * Server callbacks
 * ---------------------------------------------------------------------- */
static void
on_ask_for_password (void                  *user_data,
                     const char            *prompt,
                     ply_trigger_t         *answer,
                     ply_boot_connection_t *connection,
                     ply_boot_server_t     *server)
{
        test_state_t *state = user_data;

        if (!state->ask1_called) {
                /* First password request: stash trigger, do not answer.
                 * Simulates booster waiting for user to type while a token
                 * is simultaneously unlocking the volume. */
                state->ask1_called = true;
                state->ask1_connection = connection;
                state->ask1_trigger = answer;
        } else {
                /* Second password request arrives after first client disconnected. */
                state->ask2_called = true;
                /* Pull the trigger with NULL so the server can clean up and the
                 * event loop can exit. */
                ply_trigger_pull (answer, NULL);
                ply_event_loop_exit (state->loop, 0);
        }
}

static void
on_hangup (void                  *user_data,
           ply_boot_connection_t *connection,
           ply_boot_server_t     *server)
{
        test_state_t *state = user_data;

        if (connection != state->ask1_connection)
                return; /* not the connection we're tracking */

        state->hangup1_called = true;
        state->hangup1_connection = connection;

        /* Cancel the pending prompt by pulling the trigger with NULL.
         * This is the same sequence performed by on_connection_hangup in
         * main.c and is safe here because the connection is still live
         * (connection_hangup_handler fires *before* drop_reference). */
        if (state->ask1_trigger != NULL) {
                ply_trigger_pull (state->ask1_trigger, NULL);
                state->ask1_trigger = NULL;
        }

        /* Do not exit the loop yet; wait for the second client. */
}

/* -------------------------------------------------------------------------
 * Raw socket helpers
 * ---------------------------------------------------------------------- */
static const char *
get_test_socket_path (void)
{
        const char *path = getenv ("PLY_BOOT_SOCKET_PATH");

        if (path != NULL && path[0] != '\0')
                return path;

        return PLY_BOOT_PROTOCOL_TRIMMED_ABSTRACT_SOCKET_PATH;
}

static int
connect_to_test_socket (void)
{
        const char *path = get_test_socket_path ();
        struct sockaddr_un addr;
        socklen_t addrlen;
        int fd;

        fd = socket (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0)
                return -1;

        memset (&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;

        /* Trimmed-abstract encoding: leading NUL + path string, address
         * length covers only the bytes actually used (not the full array). */
        strncpy (addr.sun_path + 1, path, sizeof(addr.sun_path) - 2);
        addrlen = (socklen_t) (offsetof (struct sockaddr_un, sun_path) + 1 + strlen (path));

        if (connect (fd, (struct sockaddr *) &addr, addrlen) < 0) {
                close (fd);
                return -1;
        }
        return fd;
}

/* Build and send a PLY_BOOT_PROTOCOL_REQUEST_TYPE_PASSWORD frame.
 * Frame layout: [cmd '\002' uint8_size data_with_nul] */
static bool
send_password_request (int         fd,
                       const char *prompt)
{
        uint8_t frame[3 + 255];            /* argument size field is uint8, so max 255 bytes */
        size_t plen = strlen (prompt) + 1; /* NUL-terminated argument */

        if (plen > 255)
                return false;

        frame[0] = (uint8_t) PLY_BOOT_PROTOCOL_REQUEST_TYPE_PASSWORD[0]; /* '*' */
        frame[1] = '\002';                                               /* argument follows */
        frame[2] = (uint8_t) plen;
        memcpy (frame + 3, prompt, plen);

        return write (fd, frame, 3 + plen) == (ssize_t) (3 + plen);
}

/* -------------------------------------------------------------------------
 * Child process: client behaviour
 *
 * The child drives two connections in sequence so the parent's event loop
 * can observe both scenarios without any inter-process synchronisation
 * beyond the natural ordering imposed by connection lifetimes.
 * ---------------------------------------------------------------------- */
static void
run_client (void)
{
        int fd;

        /* Give the server a moment to start listening. */
        usleep (50 * 1000);

        /* --- Client 1: sends password request, then disconnects ---------- */
        fd = connect_to_test_socket ();
        if (fd < 0) {
                fprintf (stderr, "client: could not connect to test socket: %m\n");
                exit (1);
        }

        if (!send_password_request (fd, "Passphrase (client 1): ")) {
                fprintf (stderr, "client: write failed: %m\n");
                exit (1);
        }

        /* Brief pause so the server processes the request before we close. */
        usleep (150 * 1000);

        /* Close without answering — this is the scenario that triggered #125
         * and #126: the asking process (e.g. booster) exits after a token
         * unlocks the volume, leaving the Plymouth password dialog orphaned. */
        close (fd);

        /* --- Client 2: arrives after client 1's prompt was cancelled ------ */
        usleep (150 * 1000);

        fd = connect_to_test_socket ();
        if (fd < 0) {
                fprintf (stderr, "client: could not reconnect: %m\n");
                exit (1);
        }

        if (!send_password_request (fd, "Passphrase (client 2): ")) {
                fprintf (stderr, "client: second write failed: %m\n");
                exit (1);
        }

        /* Keep connection open until the server answers or we time out. */
        usleep (500 * 1000);
        close (fd);
        exit (0);
}

/* -------------------------------------------------------------------------
 * Alarm handler – safety net so the test never hangs in CI
 *
 * Only async-signal-safe calls are allowed here, so the TAP bail-out is
 * written with write() rather than printf().
 * ---------------------------------------------------------------------- */
static void
on_alarm (int sig __attribute__((unused)))
{
        static const char message[] = "Bail out! test timed out\n";

        if (write (STDOUT_FILENO, message, sizeof(message) - 1) < 0) {
                /* Nothing useful to do if the bail-out cannot be reported. */
        }
        _exit (EXIT_FAILURE);
}

/* -------------------------------------------------------------------------
 * Test cases
 *
 * These run after the scenario has played out, so they only inspect the
 * observations main() recorded.
 * ---------------------------------------------------------------------- */

/* Issue #125: the hangup callback must fire while the connection is still
 * live, with the same pointer ask_for_password_handler was given. */
static bool
test_hangup_fires_with_originating_connection (void)
{
        PLY_TEST_ASSERT (test_state.ask1_called);
        PLY_TEST_ASSERT (test_state.hangup1_called);
        PLY_TEST_ASSERT (test_state.hangup1_connection == test_state.ask1_connection);
        return true;
}

/* Issue #126: once the orphaned prompt is cancelled, the server must keep
 * serving rather than leave the dialog up for the rest of the boot.
 */
static bool
test_server_responsive_after_hangup (void)
{
        PLY_TEST_ASSERT (test_state.ask2_called);
        PLY_TEST_ASSERT (test_state.client_exited_cleanly);
        return true;
}

static const ply_test_case_t test_cases[] = {
        PLY_TEST_CASE (test_hangup_fires_with_originating_connection),
        PLY_TEST_CASE (test_server_responsive_after_hangup),
};

#define NUMBER_OF_TEST_CASES (sizeof(test_cases) / sizeof(test_cases[0]))

/* Report every case as a TAP skip.  Used when the environment cannot support
 * the test at all, so meson records SKIP instead of a failure. */
static int
skip_all_test_cases (const char *reason)
{
        printf ("TAP version 13\n");
        printf ("1..%zu\n", NUMBER_OF_TEST_CASES);

        for (size_t i = 0; i < NUMBER_OF_TEST_CASES; i++) {
                printf ("ok %zu - %s # SKIP %s\n",
                        i + 1, test_cases[i].name, reason);
        }

        return EXIT_SUCCESS;
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */
int
main (void)
{
        ply_boot_server_t *server;
        ply_event_loop_t *loop;
        char socket_path[64];
        pid_t child;
        int child_status;
        int result;

        if (getuid () != 0)
                return skip_all_test_cases ("test requires root (SO_PEERCRED uid check)");

        signal (SIGALRM, on_alarm);
        alarm (TEST_TIMEOUT_SECONDS);

        /* Unique per-run abstract socket path avoids conflicts with a running
         * plymouthd and between parallel test executions. */
        snprintf (socket_path, sizeof(socket_path),
                  "/test/ply-hangup-%d", (int) getpid ());
        setenv ("PLY_BOOT_SOCKET_PATH", socket_path, 1);

        loop = ply_event_loop_new ();
        test_state.loop = loop;

        server = ply_boot_server_new (
                NULL,                  /* update */
                NULL,                  /* change_mode */
                NULL,                  /* system_update */
                on_ask_for_password,   /* ask_for_password */
                NULL,                  /* ask_question */
                NULL,                  /* display_message */
                NULL,                  /* hide_message */
                NULL,                  /* watch_for_keystroke */
                NULL,                  /* ignore_keystroke */
                NULL,                  /* progress_pause */
                NULL,                  /* progress_unpause */
                NULL,                  /* show_splash */
                NULL,                  /* hide_splash */
                NULL,                  /* newroot */
                NULL,                  /* system_initialized */
                NULL,                  /* error */
                NULL,                  /* deactivate */
                NULL,                  /* reactivate */
                NULL,                  /* quit */
                NULL,                  /* has_active_vt */
                NULL,                  /* reload */
                on_hangup,             /* connection_hangup — the new callback */
                &test_state);

        if (!ply_boot_server_listen (server)) {
                fprintf (stderr, "could not listen on '%s': %m\n", socket_path);
                return EXIT_FAILURE;
        }

        ply_boot_server_attach_to_event_loop (server, loop);

        /* Nothing has been written to stdout yet, so the child cannot inherit
         * buffered TAP output and flush a duplicate copy of it on exit. */
        child = fork ();
        if (child < 0) {
                fprintf (stderr, "fork failed: %m\n");
                return EXIT_FAILURE;
        }
        if (child == 0)
                run_client (); /* does not return */

        /* Run until on_ask_for_password (client 2) calls ply_event_loop_exit,
         * or until the alarm fires. */
        ply_event_loop_run (loop);

        waitpid (child, &child_status, 0);
        test_state.client_exited_cleanly = WIFEXITED (child_status) &&
                                           WEXITSTATUS (child_status) == 0;

        result = ply_test_run (test_cases, NUMBER_OF_TEST_CASES);

        ply_boot_server_free (server);
        ply_event_loop_free (loop);
        alarm (0);
        return result;
}

/* ply-kmsg-reader.c - kernel log message reader
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 */
#ifndef PLY_KMSG_READER_H
#define PLY_KMSG_READER_H

#include "ply-boot-splash.h"
#include <ctype.h>
#include <sys/syslog.h>

#define from_hex(c)             (isdigit (c) ? c - '0' : tolower (c) - 'a' + 10)

typedef struct _ply_kmsg_reader ply_kmsg_reader_t;

struct dmesg_name
{
        const char *name;
};

static const struct dmesg_name priority_names[] =
{
        [LOG_EMERG] =   { "emerg"  },
        [LOG_ALERT] =   { "alert"  },
        [LOG_CRIT] =    { "crit"   },
        [LOG_ERR] =     { "err"    },
        [LOG_WARNING] = { "warn"   },
        [LOG_NOTICE] =  { "notice" },
        [LOG_INFO] =    { "info"   },
        [LOG_DEBUG] =   { "debug"  }
};


static const struct dmesg_name facility_names[] =
{
        [LOG_FAC (LOG_KERN)] =     { "kern"     },
        [LOG_FAC (LOG_USER)] =     { "user"     },
        [LOG_FAC (LOG_MAIL)] =     { "mail"     },
        [LOG_FAC (LOG_DAEMON)] =   { "daemon"   },
        [LOG_FAC (LOG_AUTH)] =     { "auth"     },
        [LOG_FAC (LOG_SYSLOG)] =   { "syslog"   },
        [LOG_FAC (LOG_LPR)] =      { "lpr"      },
        [LOG_FAC (LOG_NEWS)] =     { "news"     },
        [LOG_FAC (LOG_UUCP)] =     { "uucp"     },
        [LOG_FAC (LOG_CRON)] =     { "cron"     },
        [LOG_FAC (LOG_AUTHPRIV)] = { "authpriv" },
        [LOG_FAC (LOG_FTP)] =      { "ftp"      },
        [LOG_FAC (LOG_LOCAL0)] =   { "local0"   },
        [LOG_FAC (LOG_LOCAL1)] =   { "local1"   },
        [LOG_FAC (LOG_LOCAL2)] =   { "local2"   },
        [LOG_FAC (LOG_LOCAL3)] =   { "local3"   },
        [LOG_FAC (LOG_LOCAL4)] =   { "local4"   },
        [LOG_FAC (LOG_LOCAL5)] =   { "local5"   },
        [LOG_FAC (LOG_LOCAL6)] =   { "local6"   },
        [LOG_FAC (LOG_LOCAL7)] =   { "local7"   }
};

typedef struct
{
        int                priority;
        int                facility;
        char              *priority_name;
        char              *facility_name;
        unsigned long      sequence;
        unsigned long long timestamp;
        char              *message;
} kmsg_message_t;

struct _ply_kmsg_reader
{
        int             kmsg_fd;
        ply_fd_watch_t *fd_watch;
        ply_list_t     *kmsg_messages;
        ply_trigger_t  *kmsg_trigger;
};

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
void ply_kmsg_reader_start (ply_kmsg_reader_t *kmsg_reader);
#endif //PLY_HIDE_FUNCTION_DECLARATIONS

#endif //PLY_KMSG_READER_H

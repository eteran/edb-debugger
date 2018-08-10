/*
Copyright (C) 2015 - 2015 Evan Teran
                          evan.teran@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "PlatformCommon.h"
#include <QFile>
#include <QTextStream>
#include <QRegExp>
#include <QtDebug>
#include <sys/wait.h>

namespace DebuggerCorePlugin {

//------------------------------------------------------------------------------
// Name: resume_code
// Desc:
//------------------------------------------------------------------------------
int resume_code(int status) {

	if(WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP) {
		return 0;
	}

	if(WIFSIGNALED(status)) {
		return WTERMSIG(status);
	}

	if(WIFSTOPPED(status)) {
		return WSTOPSIG(status);
	}

	return 0;
}

//------------------------------------------------------------------------------
// Name: get_user_stat
// Desc: gets the contents of /proc/<pid>/stat and returns the number of elements
//       successfully parsed
//------------------------------------------------------------------------------
int get_user_stat(const QString &path, struct user_stat *user_stat) {
	Q_ASSERT(user_stat);

	int r = -1;
	QFile file(path);
	if(file.open(QIODevice::ReadOnly)) {
		QTextStream in(&file);
		const QString line = in.readLine();
		if(!line.isNull()) {

			// TODO(eteran): revisit this, I think that the best approach is
			// use strrchr to search for the last ')' since none of the other fields
			// will contain that character. Then use sscanf on the rest of the fields
			// starting from that location!

#if 1
			QRegExp regex(QLatin1String(R"(^(-?[0-9]+) \((.+)\) (.) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) (-?[0-9]+))"));
			int n = regex.indexIn(line);
			if(n != -1) {
				const QStringList &fields = regex.capturedTexts();

				switch(regex.captureCount()) {
				default:
				case 44: user_stat->cguest_time           = fields[44].toLongLong();  /* fallthrough */ // "%lld"
				case 43: user_stat->guest_time            = fields[43].toULongLong(); /* fallthrough */ // "%llu"
				case 42: user_stat->delayacct_blkio_ticks = fields[42].toULongLong(); /* fallthrough */ // "%llu"
				case 41: user_stat->policy                = fields[41].toUInt();      /* fallthrough */ // "%u"
				case 40: user_stat->rt_priority           = fields[40].toUInt();      /* fallthrough */ // "%u"
				case 39: user_stat->processor             = fields[39].toInt();       /* fallthrough */ // "%d"
				case 38: user_stat->exit_signal           = fields[38].toInt();       /* fallthrough */ // "%d"
				case 37: user_stat->cnswap                = fields[37].toULongLong(); /* fallthrough */ // "%llu"
				case 36: user_stat->nswap                 = fields[36].toULongLong(); /* fallthrough */ // "%llu"
				case 35: user_stat->wchan                 = fields[35].toULongLong(); /* fallthrough */ // "%llu"
				case 34: user_stat->sigcatch              = fields[34].toULongLong(); /* fallthrough */ // "%llu"
				case 33: user_stat->sigignore             = fields[33].toULongLong(); /* fallthrough */ // "%llu"
				case 32: user_stat->blocked               = fields[32].toULongLong(); /* fallthrough */ // "%llu"
				case 31: user_stat->signal                = fields[31].toULongLong(); /* fallthrough */ // "%llu"
				case 30: user_stat->kstkeip               = fields[30].toULongLong(); /* fallthrough */ // "%llu"
				case 29: user_stat->kstkesp               = fields[29].toULongLong(); /* fallthrough */ // "%llu"
				case 28: user_stat->startstack            = fields[28].toULongLong(); /* fallthrough */ // "%llu"
				case 27: user_stat->endcode               = fields[27].toULongLong(); /* fallthrough */ // "%llu"
				case 26: user_stat->startcode             = fields[26].toULongLong(); /* fallthrough */ // "%llu"
				case 25: user_stat->rsslim                = fields[25].toULongLong(); /* fallthrough */ // "%llu"
				case 24: user_stat->rss                   = fields[24].toLongLong();  /* fallthrough */ // "%lld"
				case 23: user_stat->vsize                 = fields[23].toULongLong(); /* fallthrough */ // "%llu"
				case 22: user_stat->starttime             = fields[22].toULongLong(); /* fallthrough */ // "%llu"
				case 21: user_stat->itrealvalue           = fields[21].toLongLong();  /* fallthrough */ // "%lld"
				case 20: user_stat->num_threads           = fields[20].toLongLong();  /* fallthrough */ // "%lld"
				case 19: user_stat->nice                  = fields[19].toLongLong();  /* fallthrough */ // "%lld"
				case 18: user_stat->priority              = fields[18].toLongLong();  /* fallthrough */ // "%lld"
				case 17: user_stat->cstime                = fields[17].toLongLong();  /* fallthrough */ // "%lld"
				case 16: user_stat->cutime                = fields[16].toLongLong();  /* fallthrough */ // "%lld"
				case 15: user_stat->stime                 = fields[15].toULongLong(); /* fallthrough */ // "%llu"
				case 14: user_stat->utime                 = fields[14].toULongLong(); /* fallthrough */ // "%llu"
				case 13: user_stat->cmajflt               = fields[13].toULongLong(); /* fallthrough */ // "%llu"
				case 12: user_stat->majflt                = fields[12].toULongLong(); /* fallthrough */ // "%llu"
				case 11: user_stat->cminflt               = fields[11].toULongLong(); /* fallthrough */ // "%llu"
				case 10: user_stat->minflt                = fields[10].toULongLong(); /* fallthrough */ // "%llu"
				case  9: user_stat->flags                 = fields[9].toUInt();       /* fallthrough */ // "%u"
				case  8: user_stat->tpgid                 = fields[8].toInt();        /* fallthrough */ // "%d"
				case  7: user_stat->tty_nr                = fields[7].toInt();        /* fallthrough */ // "%d"
				case  6: user_stat->session               = fields[6].toInt();        /* fallthrough */ // "%d"
				case  5: user_stat->pgrp                  = fields[5].toInt();        /* fallthrough */ // "%d"
				case  4: user_stat->ppid                  = fields[4].toInt();        /* fallthrough */ // "%d"
				case  3: user_stat->state                 = fields[3][0].toLatin1();  /* fallthrough */ // "%c"
				case  2: snprintf(user_stat->comm, sizeof(user_stat->comm), "%s", qPrintable(fields[2])); /* fallthrough */
				case  1: user_stat->pid                   = fields[1].toInt();        // "%d"
					break;
				case  0:
					qDebug() << "Warning: Failed to read any fields from /proc/<pid>/stat";
				}

				return regex.captureCount();
			}
#else
			char ch;
			r = sscanf(qPrintable(line), "%d %c%255[0-9a-zA-Z_- #~/\\.]%c %c %d %d %d %d %d %u %llu %llu %llu %llu %llu %llu %lld %lld %lld %lld %lld %lld %llu %llu %lld %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %d %d %u %u %llu %llu %lld",
					&user_stat->pid,
					&ch, // consume the (
					user_stat->comm,
					&ch, // consume the )
					&user_stat->state,
					&user_stat->ppid,
					&user_stat->pgrp,
					&user_stat->session,
					&user_stat->tty_nr,
					&user_stat->tpgid,
					&user_stat->flags,
					&user_stat->minflt,
					&user_stat->cminflt,
					&user_stat->majflt,
					&user_stat->cmajflt,
					&user_stat->utime,
					&user_stat->stime,
					&user_stat->cutime,
					&user_stat->cstime,
					&user_stat->priority,
					&user_stat->nice,
					&user_stat->num_threads,
					&user_stat->itrealvalue,
					&user_stat->starttime,
					&user_stat->vsize,
					&user_stat->rss,
					&user_stat->rsslim,
					&user_stat->startcode,
					&user_stat->endcode,
					&user_stat->startstack,
					&user_stat->kstkesp,
					&user_stat->kstkeip,
					&user_stat->signal,
					&user_stat->blocked,
					&user_stat->sigignore,
					&user_stat->sigcatch,
					&user_stat->wchan,
					&user_stat->nswap,
					&user_stat->cnswap,
					&user_stat->exit_signal,
					&user_stat->processor,
					&user_stat->rt_priority,
					&user_stat->policy,
					&user_stat->delayacct_blkio_ticks,
					&user_stat->guest_time,
					&user_stat->cguest_time);
#endif
		}
		file.close();
	}

	return r;
}

//------------------------------------------------------------------------------
// Name: get_user_stat
// Desc: gets the contents of /proc/<pid>/stat and returns the number of elements
//       successfully parsed
//------------------------------------------------------------------------------
int get_user_stat(edb::pid_t pid, struct user_stat *user_stat) {
	return get_user_stat(QString("/proc/%1/stat").arg(pid), user_stat);
}

}

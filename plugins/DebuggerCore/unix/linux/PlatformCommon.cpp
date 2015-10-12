
#include "PlatformCommon.h"
#include <QFile>
#include <QTextStream>

namespace DebuggerCore {

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
			char ch;
			r = sscanf(qPrintable(line), "%d %c%255[0-9a-zA-Z_ #~/-]%c %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld %ld %llu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %d %d %u %u %llu %lu %ld",
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

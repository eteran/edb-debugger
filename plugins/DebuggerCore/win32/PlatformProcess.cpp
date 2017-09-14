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


#include "PlatformProcess.h"

namespace DebuggerCorePlugin {

	PlatformProcess::PlatformProcess(const PROCESSENTRY32& pe) {
		_pid = pe.th32ProcessID;
		_name = QString::fromWCharArray(pe.szExeFile);

		if (HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, _pid)) {
			BOOL wow64 = FALSE;
			typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
			LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
			if (fnIsWow64Process && fnIsWow64Process(hProc, &wow64) && wow64) {
				_name += " *32";
			}

			HANDLE hToken;
			if (OpenProcessToken(hProc, TOKEN_QUERY, &hToken)) {

				DWORD needed;
				GetTokenInformation(hToken, TokenOwner, NULL, 0, &needed);

				if (auto owner = static_cast<TOKEN_OWNER *>(malloc(needed))) {
					if (GetTokenInformation(hToken, TokenOwner, owner, needed, &needed)) {
						WCHAR user[MAX_PATH];
						WCHAR domain[MAX_PATH];
						DWORD user_sz = MAX_PATH;
						DWORD domain_sz = MAX_PATH;
						SID_NAME_USE snu;

						if (LookupAccountSid(NULL, owner->Owner, user, &user_sz, domain, &domain_sz, &snu) && snu == SidTypeUser) {
							_user = QString::fromWCharArray(user);
						}
					}
					free(owner);
				}

				CloseHandle(hToken);
			}

			CloseHandle(hProc);
		}
	}

	edb::pid_t PlatformProcess::pid() const {
		return _pid;
	}

	QString PlatformProcess::name() const {
		return _name;
	}

	QString PlatformProcess::user() const {
		return _user;
	}

}

/*
Copyright (C) 2014 - 2016 Evan Teran
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
#ifndef SESSIONERROR_20170929_H_
#define SESSIONERROR_20170929_H_

#include <QString>

class SessionError {
public:
	enum ErrorCode {
		NoError            = 0,
		UnknownError       = 1,
		NotAnObject        = 2,
		InvalidSessionFile = 3,
	};

public:
	QString getErrorMessage() const;
	void setErrorMessage(const QString);

public:
	ErrorCode err = NoError;

private:
	QString errorMessage = "";
};

#endif

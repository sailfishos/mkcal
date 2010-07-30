/****************************************************************************
**
** This file is part of a Qt Solutions component.
** 
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
** 
** Contact:  Qt Software Information (qt-info@nokia.com)
** 
** Commercial Usage  
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Solutions Commercial License Agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and Nokia.
** 
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
** 
** In addition, as a special exception, Nokia gives you certain
** additional rights. These rights are described in the Nokia Qt LGPL
** Exception version 1.0, included in the file LGPL_EXCEPTION.txt in this
** package.
** 
** GNU General Public License Usage 
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
** 
** Please note Third Party Software included with Qt Solutions may impose
** additional restrictions and it is the user's responsibility to ensure
** that they have met the licensing requirements of the GPL, LGPL, or Qt
** Solutions Commercial license and the relevant license of the Third
** Party Software they are using.
** 
** If you are unsure which license is appropriate for your use, please
** contact the sales department at qt-sales@nokia.com.
** 
****************************************************************************/

#include <stdio.h>
#include <QtCore/QTextStream>
#include <qtlockedfile.h>


int main()
{
    QTextStream qout(stdout);
    QTextStream qin(stdin);

    qout << "---===>>>> File locking example <<<<===---\n";

    QtLockedFile lf("foo");
    lf.open(QFile::ReadWrite);

    QString line;
    bool blocking = true;
    while (line != "q") {
        int m = lf.lockMode();

        if (m == 0)
            qout << "\n[*] You have no locks.";
        else if (m == QFile::ReadOnly)
            qout << "\n[*] You have a read lock.";
        else
            qout << "\n[*] You have a read/write lock.";
        qout << " Blocking wait is ";
        if (blocking)
            qout << "ON.\n";
        else
            qout << "OFF.\n";

        qout << "Acquire [r]ead lock, read/[w]rite lock, re[l]ease lock, [t]oggle or [q]uit? ";
        qout.flush();

        line = qin.readLine();
        if (line.isNull())
            break;

        if (line == "r") {
            qout << "Acquiring a read lock... ";
            qout.flush();
            if (lf.lock(QtLockedFile::ReadLock, blocking))
                qout << "done!\n";
            else
                qout << "not currently possible!\n";
        } else if (line == "w") {
            qout << "Acquiring a read/write lock... ";
            qout.flush();
            if (lf.lock(QtLockedFile::WriteLock, blocking))
                qout << "done!\n";
            else
                qout << "not currently possible!\n";
        } else if (line == "l") {
            qout << "Releasing lock... ";
            qout.flush();
            lf.unlock();
            qout << "done!\n";
        } else if (line == "t") {
            blocking = !blocking;
        }

        qout.flush();
    }
}

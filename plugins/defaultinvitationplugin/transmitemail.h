/* * This file is part of Harmattan Organiser GUI *
 *
 * Copyright (C) 2009-2010 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 *
 * Contact: Andrey Moiseenko <andrey.moiseenko@nokia.com>
 *
 * This software, including documentation, is protected by copyright
 * controlled by Nokia Corporation. All rights are reserved. Copying,
 * including reproducing, storing, adapting or translating, any or all of
 * this material requires the prior written consent of Nokia Corporation.
 * This material also contains confidential information which may not be
 * disclosed to others without the prior written consent of Nokia.
 */

#ifndef TRANSMITEMAIL_H
#define TRANSMITEMAIL_H

#include <QObject>

#include <qmailaccount.h>
#include <qmailserviceaction.h>

class TransmitHelper : public QObject
{
	Q_OBJECT
public:
	TransmitHelper(QMailAccountId id);

public slots:
	void transmit();
	void changeActivity(QMailServiceAction::Activity a);

signals:
	void done();

private:
	QMailTransmitAction* mAction;
	QMailAccountId mId;
};

#endif // TRANSMITEMAIL_H

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


#include "transmitemail.h"

TransmitHelper::TransmitHelper(QMailAccountId id) : mId(id)
{
    mAction = new QMailTransmitAction(this);
    Q_ASSERT(mAction);

    connect(mAction, SIGNAL(activityChanged(QMailServiceAction::Activity)), this, SLOT(changeActivity(QMailServiceAction::Activity)));
}

void TransmitHelper::transmit()
{
    qDebug() << "TransmitHelper::transmit()" << mId;
	mAction->transmitMessages(mId);
}

void TransmitHelper::changeActivity(QMailServiceAction::Activity a)
{
    switch (a)
    {
        case QMailServiceAction::Pending:
            qDebug() << "Pending request to server...";
            break;

        case QMailServiceAction::InProgress:
            qDebug() << "Request to server in progress...";
            break;

        case QMailServiceAction::Successful:
            qDebug() << "Request to server has been completed successfull!";
            emit done();
            break;

        case QMailServiceAction::Failed:
            qDebug() << "Request to server failed!";
            emit done();
            break;

        default:
            Q_ASSERT(false);
            break;
    }
}

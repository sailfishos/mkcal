/*
  Copyright (c) 2022 Damien Caliste <dcaliste@free.fr>.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#include <QTest>
#include <QDebug>
#include <QObject>
#include <QSignalSpy>

#include "extendedstorageobserver.h"

#include "tst_async.h"

class Observer: public QObject, public mKCal::ExtendedStorageObserver
{
    Q_OBJECT
public:
    Observer(ExtendedStorage::Ptr storage): mStorage(storage)
    {
        mStorage->registerObserver(this);
    }
    ~Observer()
    {
        mStorage->unregisterObserver(this);
    }
    void storageOpened(ExtendedStorage *storage) override
    {
        emit opened();
    }
    void storageClosed(ExtendedStorage *storage) override
    {
        emit closed();
    }
    void storageModified(ExtendedStorage *storage, const QString &info) override
    {
        emit modified();
    }
    void storageUpdated(ExtendedStorage *storage,
                        const KCalendarCore::Incidence::List &added,
                        const KCalendarCore::Incidence::List &modified,
                        const KCalendarCore::Incidence::List &deleted) override
    {
        emit updated(added, modified, deleted);
    }
    void storageLoaded(ExtendedStorage *storage,
                       const KCalendarCore::Incidence::List &incidences) override
    {
        emit loaded(incidences);
    }

signals:
    void opened();
    void closed();
    void modified();
    void updated(const KCalendarCore::Incidence::List &added,
                 const KCalendarCore::Incidence::List &modified,
                 const KCalendarCore::Incidence::List &deleted);
    void loaded(const KCalendarCore::Incidence::List &incidences);
private:
    ExtendedStorage::Ptr mStorage;
};

tst_async::tst_async(QObject *parent)
    : QObject(parent)
    , db(nullptr)
{
}

void tst_async::initTestCase()
{
    QString dbFile = QString::fromLatin1(qgetenv("SQLITESTORAGEDB"));
    if (dbFile.isEmpty()) {
        db = new QTemporaryFile();
        db->open();
        dbFile = db->fileName();
    }
    ExtendedCalendar::Ptr cal(new ExtendedCalendar(QTimeZone::systemTimeZone()));
    m_storage = ExtendedStorage::Ptr(new AsyncSqliteStorage(cal, dbFile, true));
}

void tst_async::cleanupTestCase()
{
    m_storage.clear();
    if (db)
        QFile::remove(db->fileName() + ".changed");
    delete db;
}

void tst_async::init()
{
    Observer observer(m_storage);
    QSignalSpy opened(&observer, &Observer::opened);
    QVERIFY(m_storage->calendar()->rawEvents().isEmpty());
    QVERIFY(opened.isEmpty());
    QVERIFY(m_storage->open());
    QVERIFY(opened.wait(2000));
    QCOMPARE(opened.count(), 1);
}

void tst_async::cleanup()
{
    Observer observer(m_storage);
    QSignalSpy closed(&observer, &Observer::closed);
    QVERIFY(closed.isEmpty());
    QVERIFY(m_storage->close());
    QVERIFY(closed.wait(2000));
    QCOMPARE(closed.count(), 1);
    m_storage->calendar()->close();
    QVERIFY(m_storage->calendar()->rawEvents().isEmpty());
}

Q_DECLARE_METATYPE(KCalendarCore::Incidence::List);
void tst_async::tst_save()
{
    const QDateTime at(QDate(2022, 11, 28), QTime(11, 1));
    Observer observer(m_storage);
    QSignalSpy updated(&observer, &Observer::updated);

    // External observer
    ExtendedCalendar::Ptr calendar(new ExtendedCalendar("UTC"));
    AsyncSqliteStorage::Ptr storage(new AsyncSqliteStorage(calendar));
    Observer externalObserver(storage);
    QSignalSpy opened(&externalObserver, &Observer::opened);
    QVERIFY(storage->open());
    QVERIFY(opened.wait(2000));
    QSignalSpy modified(&externalObserver, &Observer::modified);

    // Adding an event to the DB.
    KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
    event->setDtStart(at);
    QVERIFY(m_storage->calendar()->addIncidence(event));
    QVERIFY(m_storage->calendar()->incidence(event->uid()));
    QVERIFY(updated.isEmpty());
    QVERIFY(m_storage->save());
    QVERIFY(updated.isEmpty());
    QVERIFY(updated.wait(2000));
    QCOMPARE(updated.count(), 1);
    QList<QVariant> args = updated.takeFirst();
    QCOMPARE(args.count(), 3);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>().count(), 1);
    QVERIFY(args[1].value<KCalendarCore::Incidence::List>().isEmpty());
    QVERIFY(args[2].value<KCalendarCore::Incidence::List>().isEmpty());
    KCalendarCore::Incidence::Ptr addition = args[0].value<KCalendarCore::Incidence::List>()[0];
    QVERIFY(addition);
    QCOMPARE(*addition.staticCast<KCalendarCore::Event>(), *event);
    QVERIFY(!modified.isEmpty() || modified.wait(2000)); // See the external modification
    QVERIFY(!opened.isEmpty() || opened.wait(2000)); // Reopen on external modification
    modified.clear();
    opened.clear();

    // Updating it and deleting it in a row.
    event->setDtStart(at.addDays(-1));
    QVERIFY(updated.isEmpty());
    QVERIFY(m_storage->save());
    QVERIFY(updated.isEmpty());
    QVERIFY(m_storage->calendar()->deleteIncidence(event));
    QVERIFY(m_storage->save(ExtendedStorage::PurgeDeleted));
    QVERIFY(updated.isEmpty());
    QVERIFY(updated.wait(2000)); // Wait for the update on modification
    QVERIFY(updated.wait(2000)); // Wait for the update on deletion
    QCOMPARE(updated.count(), 2);
    args = updated.takeFirst();
    QCOMPARE(args.count(), 3);
    QVERIFY(args[0].value<KCalendarCore::Incidence::List>().isEmpty());
    QVERIFY(args[1].value<KCalendarCore::Incidence::List>().isEmpty()); // The modified incidence has already been deleted
    QVERIFY(args[2].value<KCalendarCore::Incidence::List>().isEmpty());
    args = updated.takeFirst();
    QCOMPARE(args.count(), 3);
    QVERIFY(args[0].value<KCalendarCore::Incidence::List>().isEmpty());
    QVERIFY(args[1].value<KCalendarCore::Incidence::List>().isEmpty());
    QCOMPARE(args[2].value<KCalendarCore::Incidence::List>().count(), 1);
    KCalendarCore::Incidence::Ptr deletion = args[2].value<KCalendarCore::Incidence::List>()[0];
    QVERIFY(deletion);
    QCOMPARE(deletion->uid(), event->uid());
    QVERIFY(!modified.isEmpty() || modified.wait(2000)); // See the external modification
    QVERIFY(!opened.isEmpty() || opened.wait(2000)); // Reopen on external modification
}

void tst_async::tst_notebook()
{
    Notebook::Ptr nb(new Notebook(QString::fromLatin1("Test async"), QString()));
    Observer observer(m_storage);
    QSignalSpy mainModified(&observer, &Observer::modified);

    ExtendedCalendar::Ptr calendar(new ExtendedCalendar("UTC"));
    AsyncSqliteStorage::Ptr storage(new AsyncSqliteStorage(calendar));
    Observer externalObserver(storage);
    QSignalSpy modified(&externalObserver, &Observer::modified);
    QSignalSpy opened(&externalObserver, &Observer::opened);
    QVERIFY(storage->open());
    QVERIFY(opened.wait(2000));

    QVERIFY(m_storage->addNotebook(nb));
    QVERIFY(m_storage->calendar()->hasValidNotebook(nb->uid()));
    QVERIFY(modified.wait(2000)); // See the external modification
    QVERIFY(opened.wait(2000)); // Reopen on external modification
    QVERIFY(storage->notebook(nb->uid()));
    QCOMPARE(*storage->notebook(nb->uid()), *nb);
    QVERIFY(mainModified.isEmpty());

    const QString description = QString::fromLatin1("new description");
    nb->setDescription(description);
    nb->setIsVisible(false);
    QVERIFY(m_storage->updateNotebook(nb));
    QVERIFY(!m_storage->calendar()->isVisible(nb->uid()));
    QVERIFY(modified.wait(2000)); // See the external modification
    QVERIFY(opened.wait(2000)); // Reopen on external modification
    QVERIFY(storage->notebook(nb->uid()));
    QCOMPARE(*storage->notebook(nb->uid()), *nb);
    QVERIFY(mainModified.isEmpty());

    // Add an incidence to the new notebook to check
    // that it is removed on notebook deletion.
    KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
    event->setDtStart(QDateTime(QDate(2022, 11, 28), QTime(13, 50)));
    QVERIFY(m_storage->calendar()->addIncidence(event));
    QVERIFY(m_storage->calendar()->setNotebook(event, nb->uid()));
    QVERIFY(m_storage->save());
    QVERIFY(modified.wait(2000));
    QCOMPARE(m_storage->calendar()->incidences(nb->uid()).count(), 1);

    QVERIFY(m_storage->deleteNotebook(nb));
    QVERIFY(!m_storage->calendar()->hasValidNotebook(nb->uid()));
    QVERIFY(!m_storage->calendar()->incidence(event->uid()));
    QVERIFY(modified.wait(2000)); // See the external modification
    QVERIFY(opened.wait(2000)); // Reopen on external modification
    QVERIFY(!storage->notebook(nb->uid()));
    QVERIFY(mainModified.isEmpty());
    KCalendarCore::Incidence::List all;
    QVERIFY(m_storage->allIncidences(&all, nb->uid()));
    QVERIFY(all.isEmpty());
}

void tst_async::tst_listing()
{
    const QDateTime created(QDate(2022, 11, 28), QTime(13, 50));
    const QDateTime modified(QDate(2022, 11, 28), QTime(13, 55));
    Observer observer(m_storage);
    QSignalSpy updated(&observer, &Observer::updated);

    KCalendarCore::Incidence::List list;
    QVERIFY(m_storage->allIncidences(&list, m_storage->defaultNotebook()->uid()));
    QVERIFY(list.isEmpty());
    QVERIFY(m_storage->allIncidences(&list));
    QVERIFY(list.isEmpty());

    KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
    event->setCreated(created);
    event->setLastModified(modified);
    event->setDtStart(QDateTime(QDate(2022, 11, 28), QTime(14, 23)));
    QVERIFY(m_storage->calendar()->addIncidence(event));
    KCalendarCore::Event::Ptr event2(new KCalendarCore::Event);
    event2->setCreated(created);
    event2->setLastModified(modified);
    event2->setDtStart(QDateTime(QDate(2022, 11, 28), QTime(14, 23)));
    QVERIFY(m_storage->calendar()->addIncidence(event2));
    QVERIFY(m_storage->save());
    QVERIFY(updated.wait(2000));
    QVERIFY(m_storage->calendar()->deleteIncidence(event2));
    QVERIFY(m_storage->save());
    QVERIFY(updated.wait(2000));

    // All incidences
    list.clear();
    QVERIFY(m_storage->allIncidences(&list, m_storage->defaultNotebook()->uid()));
    QCOMPARE(list.count(), 1);
    QCOMPARE(list[0]->uid(), event->uid());
    list.clear();
    QVERIFY(m_storage->allIncidences(&list));
    QCOMPARE(list.count(), 1);
    QCOMPARE(list[0]->uid(), event->uid());

    // Inserted incidences since
    list.clear();
    QVERIFY(m_storage->insertedIncidences(&list, created,
                                          m_storage->defaultNotebook()->uid()));
    QCOMPARE(list.count(), 1);
    QCOMPARE(list[0]->uid(), event->uid());
    list.clear();
    QVERIFY(m_storage->insertedIncidences(&list, created));
    QCOMPARE(list.count(), 1);
    QCOMPARE(list[0]->uid(), event->uid());
    list.clear();
    QVERIFY(m_storage->insertedIncidences(&list, created.addSecs(1),
                                          m_storage->defaultNotebook()->uid()));
    QVERIFY(list.isEmpty());
    list.clear();
    QVERIFY(m_storage->insertedIncidences(&list, created.addSecs(1)));
    QVERIFY(list.isEmpty());

    // Modified incidences since
    list.clear();
    QVERIFY(m_storage->modifiedIncidences(&list, modified,
                                          m_storage->defaultNotebook()->uid()));
    QCOMPARE(list.count(), 1);
    QCOMPARE(list[0]->uid(), event->uid());
    list.clear();
    QVERIFY(m_storage->modifiedIncidences(&list, modified));
    QCOMPARE(list.count(), 1);
    QCOMPARE(list[0]->uid(), event->uid());
    list.clear();
    QVERIFY(m_storage->modifiedIncidences(&list, modified.addSecs(1),
                                          m_storage->defaultNotebook()->uid()));
    QVERIFY(list.isEmpty());
    list.clear();
    QVERIFY(m_storage->modifiedIncidences(&list, modified.addSecs(1)));
    QVERIFY(list.isEmpty());

    // Deleted incidences since
    const QDateTime now = QDateTime::currentDateTimeUtc();
    list.clear();
    QVERIFY(m_storage->deletedIncidences(&list, now.addSecs(-5),
                                         m_storage->defaultNotebook()->uid()));
    QCOMPARE(list.count(), 1);
    QCOMPARE(list[0]->uid(), event2->uid());
    list.clear();
    QVERIFY(m_storage->deletedIncidences(&list, now.addSecs(-5)));
    QCOMPARE(list.count(), 1);
    QCOMPARE(list[0]->uid(), event2->uid());
    list.clear();
    QVERIFY(m_storage->deletedIncidences(&list, now.addSecs(1),
                                         m_storage->defaultNotebook()->uid()));
    QVERIFY(list.isEmpty());
    list.clear();
    QVERIFY(m_storage->deletedIncidences(&list, now.addSecs(1)));
    QVERIFY(list.isEmpty());

    QVERIFY(m_storage->purgeDeletedIncidences(KCalendarCore::Incidence::List() << event2));
    QVERIFY(m_storage->calendar()->deleteIncidence(event));
    QVERIFY(m_storage->save(ExtendedStorage::PurgeDeleted));
    QVERIFY(updated.wait(2000));
}

void tst_async::tst_load()
{
    Observer observer(m_storage);
    QSignalSpy updated(&observer, &Observer::updated);
    QSignalSpy loaded(&observer, &Observer::loaded);
    
    KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
    event->setDtStart(QDateTime(QDate(2022, 11, 28), QTime(14, 23)));
    QVERIFY(m_storage->calendar()->addIncidence(event));
    KCalendarCore::Event::Ptr event2(new KCalendarCore::Event);
    event2->setDtStart(QDateTime(QDate(2022, 11, 27), QTime(15, 49)));
    event2->recurrence()->setDaily(1);
    QVERIFY(m_storage->calendar()->addIncidence(event2));
    KCalendarCore::Event::Ptr event3(event2->clone());
    event3->setDtStart(QDateTime(QDate(2022, 11, 29), QTime(16, 17)));
    event3->clearRecurrence();
    event3->setRecurrenceId(QDateTime(QDate(2022, 11, 29), QTime(15, 49)));
    QVERIFY(m_storage->calendar()->addIncidence(event3));
    QVERIFY(m_storage->save());
    QVERIFY(updated.wait(2000));

    m_storage->calendar()->close();
    QVERIFY(m_storage->calendar()->incidences().isEmpty());
    QVERIFY(m_storage->load());
    QVERIFY(loaded.isEmpty());
    QVERIFY(loaded.wait(2000));
    QCOMPARE(loaded.count(), 1);
    QList<QVariant> args = loaded.takeFirst();
    QCOMPARE(args.count(), 1);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>().count(), 3);

    m_storage->calendar()->close();
    QVERIFY(m_storage->calendar()->incidences().isEmpty());
    QVERIFY(m_storage->load(event->uid()));
    QVERIFY(m_storage->load(event2->uid()));
    QVERIFY(m_storage->load(event3->uid(), event3->recurrenceId()));
    QVERIFY(loaded.isEmpty());
    QVERIFY(loaded.wait(2000));
    QVERIFY(loaded.wait(2000));
    QVERIFY(loaded.wait(2000));
    QCOMPARE(loaded.count(), 3);
    args = loaded.takeFirst();
    QCOMPARE(args.count(), 1);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>().count(), 1);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>()[0]->uid(), event->uid());
    args = loaded.takeFirst();
    QCOMPARE(args.count(), 1);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>().count(), 1);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>()[0]->uid(), event2->uid());
    QVERIFY(args[0].value<KCalendarCore::Incidence::List>()[0]->recurs());
    args = loaded.takeFirst();
    QCOMPARE(args.count(), 1);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>().count(), 1);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>()[0]->uid(), event3->uid());
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>()[0]->recurrenceId(), event3->recurrenceId());

    m_storage->calendar()->close();
    QVERIFY(m_storage->calendar()->incidences().isEmpty());
    QVERIFY(m_storage->loadSeries(event2->uid()));
    QVERIFY(loaded.isEmpty());
    QVERIFY(loaded.wait(2000));
    QCOMPARE(loaded.count(), 1);
    args = loaded.takeFirst();
    QCOMPARE(args.count(), 1);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>().count(), 2);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>()[0]->uid(), event2->uid());
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>()[1]->uid(), event2->uid());
    if (args[0].value<KCalendarCore::Incidence::List>()[0]->recurs()) {
        QCOMPARE(args[0].value<KCalendarCore::Incidence::List>()[1]->recurrenceId(), event3->recurrenceId());
    } else {
        QCOMPARE(args[0].value<KCalendarCore::Incidence::List>()[0]->recurrenceId(), event3->recurrenceId());
    }

    m_storage->calendar()->close();
    QVERIFY(m_storage->calendar()->incidences().isEmpty());
    QVERIFY(m_storage->loadNotebookIncidences(m_storage->defaultNotebook()->uid()));
    QVERIFY(loaded.isEmpty());
    QVERIFY(loaded.wait(2000));
    QCOMPARE(loaded.count(), 1);
    args = loaded.takeFirst();
    QCOMPARE(args.count(), 1);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>().count(), 3);

    m_storage->calendar()->close();
    QVERIFY(m_storage->calendar()->incidences().isEmpty());
    QVERIFY(m_storage->loadRecurringIncidences());
    QVERIFY(loaded.isEmpty());
    QVERIFY(loaded.wait(2000));
    QCOMPARE(loaded.count(), 1);
    args = loaded.takeFirst();
    QCOMPARE(args.count(), 1);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>().count(), 2);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>()[0]->uid(), event2->uid());
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>()[1]->uid(), event2->uid());
    if (args[0].value<KCalendarCore::Incidence::List>()[0]->recurs()) {
        QCOMPARE(args[0].value<KCalendarCore::Incidence::List>()[1]->recurrenceId(), event3->recurrenceId());
    } else {
        QCOMPARE(args[0].value<KCalendarCore::Incidence::List>()[0]->recurrenceId(), event3->recurrenceId());
    }

    m_storage->calendar()->close();
    QVERIFY(m_storage->calendar()->incidences().isEmpty());
    QVERIFY(m_storage->load(QDate(2022,11,28)));
    QVERIFY(loaded.isEmpty());
    QVERIFY(loaded.wait(2000));
    QVERIFY(loaded.wait(2000));
    QCOMPARE(loaded.count(), 2); // The recursive incidences and the date load
    args = loaded.takeFirst();
    QCOMPARE(args.count(), 1);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>().count(), 2);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>()[0]->uid(), event2->uid());
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>()[1]->uid(), event2->uid());
    args = loaded.takeFirst();
    QCOMPARE(args.count(), 1);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>().count(), 1);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>()[0]->uid(), event->uid());

    QVERIFY(m_storage->calendar()->deleteIncidence(event));
    QVERIFY(m_storage->calendar()->deleteIncidence(event2));
    QVERIFY(m_storage->save(ExtendedStorage::PurgeDeleted));
    QVERIFY(updated.wait(2000));
}

void tst_async::tst_batchLoad()
{
    Observer observer(m_storage);
    QSignalSpy updated(&observer, &Observer::updated);
    QSignalSpy loaded(&observer, &Observer::loaded);
    
    KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
    event->setDtStart(QDateTime(QDate(2022, 11, 28), QTime(14, 23)));
    QVERIFY(m_storage->calendar()->addIncidence(event));
    KCalendarCore::Event::Ptr event2(new KCalendarCore::Event);
    event2->setDtStart(QDateTime(QDate(2022, 11, 27), QTime(15, 49)));
    event2->recurrence()->setDaily(1);
    QVERIFY(m_storage->calendar()->addIncidence(event2));
    KCalendarCore::Event::Ptr event3(event2->clone());
    event3->setDtStart(QDateTime(QDate(2022, 11, 29), QTime(16, 17)));
    event3->clearRecurrence();
    event3->setRecurrenceId(QDateTime(QDate(2022, 11, 29), QTime(15, 49)));
    QVERIFY(m_storage->calendar()->addIncidence(event3));
    QVERIFY(m_storage->save());
    QVERIFY(updated.wait(2000));
    m_storage->calendar()->close();

    m_storage->startBatchLoading();
    QVERIFY(m_storage->load(QDate(2022,11,27)));
    QVERIFY(m_storage->load(event->uid()));
    m_storage->runBatchLoading();
    QVERIFY(loaded.wait(2000));
    QVERIFY(!loaded.wait(200)); // Only one load signal.
    QCOMPARE(loaded.count(), 1);
    QList<QVariant> args = loaded.takeFirst();
    QCOMPARE(args.count(), 1);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>().count(), 3);

    QVERIFY(m_storage->calendar()->deleteIncidence(event));
    QVERIFY(m_storage->calendar()->deleteIncidence(event2));
    QVERIFY(m_storage->save(ExtendedStorage::PurgeDeleted));
    QVERIFY(updated.wait(2000));
}

class DirectObserver: public DirectStorageInterface::Observer
{
public:
    DirectObserver() {}
    ~DirectObserver() {}
    void storageIncidenceAdded(DirectStorageInterface *storage,
                               const KCalendarCore::Calendar *calendar,
                               const KCalendarCore::Incidence::List &added) override
    {
        isIncidenceAddedOK = added.count() == 3;
        isIncidenceAddedOK = isIncidenceAddedOK && calendar->incidences().count() == 3;
    }
    void storageIncidenceModified(DirectStorageInterface *storage,
                                  const KCalendarCore::Calendar *calendar,
                                  const KCalendarCore::Incidence::List &modified) override
    {
        isIncidenceModifiedOK = modified.count() == 1;
        isIncidenceModifiedOK = isIncidenceModifiedOK && modified[0]->hasRecurrenceId();
        isIncidenceModifiedOK = isIncidenceModifiedOK && calendar->incidences().count() == 2;
        isIncidenceModifiedOK = isIncidenceModifiedOK && calendar->incidence(modified[0]->uid());
        isIncidenceModifiedOK = isIncidenceModifiedOK && calendar->instances(calendar->incidence(modified[0]->uid())).count() == 1;
    }
    void storageIncidenceDeleted(DirectStorageInterface *storage,
                                 const KCalendarCore::Calendar *calendar,
                                 const KCalendarCore::Incidence::List &deleted) override
    {
        isIncidenceDeletedOK = deleted.count() == 1;
        isIncidenceDeletedOK = isIncidenceDeletedOK && calendar->incidences().count() == 1;
        isIncidenceDeletedOK = isIncidenceDeletedOK && storage->purgeDeletedIncidences(deleted);
    }

    bool isIncidenceAddedOK = false;
    bool isIncidenceModifiedOK = false;
    bool isIncidenceDeletedOK = false;
};

void tst_async::tst_directObserver()
{
    Observer main(m_storage);
    QSignalSpy updated(&main, &Observer::updated);

    KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
    event->setDtStart(QDateTime(QDate(2022, 11, 28), QTime(14, 23)));
    KCalendarCore::Event::Ptr event2(new KCalendarCore::Event);
    event2->setDtStart(QDateTime(QDate(2022, 11, 27), QTime(15, 49)));
    event2->recurrence()->setDaily(1);
    KCalendarCore::Event::Ptr event3(event2->clone());
    event3->setDtStart(QDateTime(QDate(2022, 11, 29), QTime(16, 17)));
    event3->clearRecurrence();
    event3->setRecurrenceId(QDateTime(QDate(2022, 11, 29), QTime(15, 49)));

    DirectObserver observer;
    m_storage->registerDirectObserver(&observer);

    QVERIFY(m_storage->calendar()->addIncidence(event));
    QVERIFY(m_storage->calendar()->addIncidence(event2));
    QVERIFY(m_storage->calendar()->addIncidence(event3));
    QVERIFY(m_storage->save());
    QVERIFY(updated.wait(2000));
    QVERIFY(observer.isIncidenceAddedOK);

    event3->setDtStart(QDateTime(QDate(2022, 11, 29), QTime(17, 17)));
    QVERIFY(m_storage->save());
    QVERIFY(updated.wait(2000));
    QVERIFY(observer.isIncidenceModifiedOK);

    QVERIFY(m_storage->calendar()->deleteIncidence(event3));
    QVERIFY(m_storage->save());
    QVERIFY(updated.wait(2000));
    QVERIFY(observer.isIncidenceDeletedOK);
    KCalendarCore::Incidence::List list;
    QVERIFY(m_storage->deletedIncidences(&list, QDateTime::currentDateTimeUtc().addSecs(-5),
                                         m_storage->defaultNotebook()->uid()));
    QVERIFY(list.isEmpty());
    QVERIFY(m_storage->allIncidences(&list, m_storage->defaultNotebook()->uid()));
    QCOMPARE(list.count(), 2);

    m_storage->unregisterDirectObserver(&observer);

    QVERIFY(m_storage->calendar()->deleteIncidence(event));
    QVERIFY(m_storage->calendar()->deleteIncidence(event2));
    QVERIFY(m_storage->save(ExtendedStorage::PurgeDeleted));
    QVERIFY(updated.wait(2000));
}

#include "tst_async.moc"

QTEST_GUILESS_MAIN(tst_async)

/*
  Copyright (c) 2023 Damien Caliste <dcaliste@free.fr>.

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

#include <QObject>
#include <QTest>
#include <QDebug>
#include <QSignalSpy>

#include <KCalendarCore/Event>

#include "singlesqlitebackend_p.h"

using namespace KCalendarCore;
using namespace mKCal;

class tst_backend: public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testNotebooks();
    void testUpdateNotebook();
    void testDeleteNotebook();

    void testSaveIncidence();
    void testDeleteIncidence();
    void testPurgeIncidence();
    void testPurgeOnAddIncidence();
    void testDeferSaveIncidence();

    void testSingleNotebookFetch();
    void testMultiNotebookFetch();

    void testPurgeOnNotebookDeletion();

private:
    SingleSqliteBackend mBackend;
    Notebook::Ptr mNotebook;
};

void tst_backend::init()
{
    QVERIFY(mBackend.open());

    mNotebook = Notebook::Ptr(new Notebook(QString::fromLatin1("Test"), QString()));
    QVERIFY(mBackend.addNotebook(*mNotebook, false));
}

void tst_backend::cleanup()
{
    QVERIFY(mBackend.deleteNotebook(*mNotebook));
    QVERIFY(mBackend.close());
    mNotebook.clear();
}

void tst_backend::testNotebooks()
{
    Notebook::List notebooks;
    QVERIFY(mBackend.notebooks(&notebooks, nullptr));

    QVERIFY(notebooks.count() == 1);
    QCOMPARE(*notebooks[0], *mNotebook);
}

void tst_backend::testUpdateNotebook()
{
    mNotebook->setColor(QString::fromLatin1("red"));
    QVERIFY(mBackend.updateNotebook(*mNotebook, false));

    Notebook::List notebooks;
    QVERIFY(mBackend.notebooks(&notebooks, nullptr));
    QVERIFY(notebooks.count() == 1);
    QCOMPARE(*notebooks[0], *mNotebook);
}

void tst_backend::testDeleteNotebook()
{
    Notebook notebook(QString::fromLatin1("Deleted notebook"), QString());
    QVERIFY(mBackend.addNotebook(notebook, false));

    Notebook::List notebooks;
    QVERIFY(mBackend.notebooks(&notebooks, nullptr));
    QVERIFY(notebooks.count() == 2);

    QVERIFY(mBackend.deleteNotebook(notebook));

    notebooks.clear();
    QVERIFY(mBackend.notebooks(&notebooks, nullptr));
    QVERIFY(notebooks.count() == 1);
    QCOMPARE(*notebooks[0], *mNotebook);
}

void tst_backend::testSaveIncidence()
{
    Event event;

    // Add an event.
    qRegisterMetaType<QHash<QString, QStringList>>();
    QSignalSpy updated(&mBackend, &SingleSqliteBackend::updated);
    QVERIFY(mBackend.addIncidence(mNotebook->uid(), event));
    QCOMPARE(updated.count(), 1);

    Incidence::List incidences;
    QVERIFY(mBackend.incidences(&incidences, mNotebook->uid(), event.uid()));
    QCOMPARE(incidences.count(), 1);
    QCOMPARE(event.uid(), incidences[0]->uid());

    const QList<QVariant> args = updated.takeFirst();
    QCOMPARE(args.count(), 3);
    const QHash<QString, QStringList> added = args[0].value<QHash<QString, QStringList>>();
    const QHash<QString, QStringList> modified = args[1].value<QHash<QString, QStringList>>();
    const QHash<QString, QStringList> deleted = args[2].value<QHash<QString, QStringList>>();
    QCOMPARE(added.count(), 1);
    QVERIFY(modified.isEmpty());
    QVERIFY(deleted.isEmpty());
    QVERIFY(added.contains(mNotebook->uid()));
    QCOMPARE(added.value(mNotebook->uid())[0], event.instanceIdentifier());

    // Can't add an existing event
    QVERIFY(!mBackend.addIncidence(mNotebook->uid(), event));
    QCOMPARE(updated.count(), 0);

    event.setSummary(QString::fromLatin1("testing change"));

    // Modify an event
    QVERIFY(mBackend.modifyIncidence(mNotebook->uid(), event));
    QCOMPARE(updated.count(), 1);

    incidences.clear();
    QVERIFY(mBackend.incidences(&incidences, mNotebook->uid(), event.uid()));
    QCOMPARE(incidences.count(), 1);
    QCOMPARE(event.summary(), incidences[0]->summary());

    const QList<QVariant> args2 = updated.takeFirst();
    QCOMPARE(args.count(), 3);
    const QHash<QString, QStringList> added2 = args2[0].value<QHash<QString, QStringList>>();
    const QHash<QString, QStringList> modified2 = args2[1].value<QHash<QString, QStringList>>();
    const QHash<QString, QStringList> deleted2 = args2[2].value<QHash<QString, QStringList>>();
    QVERIFY(added2.isEmpty());
    QCOMPARE(modified2.count(), 1);
    QVERIFY(deleted2.isEmpty());
    QVERIFY(modified2.contains(mNotebook->uid()));
    QCOMPARE(modified2.value(mNotebook->uid())[0], event.instanceIdentifier());

    Event event2;

    // Can't modify a non-existing event
    // This is emitting a warning.
    QVERIFY(!mBackend.modifyIncidence(mNotebook->uid(), event2));
    QCOMPARE(updated.count(), 0);

    // Cleanup
    QVERIFY(mBackend.purgeIncidence(mNotebook->uid(), event));
}    

void tst_backend::testDeleteIncidence()
{
    Event event, event2;

    // Add an event.
    QVERIFY(mBackend.addIncidence(mNotebook->uid(), event));

    Incidence::List incidences;
    QVERIFY(mBackend.incidences(&incidences, mNotebook->uid(), event.uid()));
    QCOMPARE(incidences.count(), 1);
    QCOMPARE(event.uid(), incidences[0]->uid());

    // Mark an event as deleted
    qRegisterMetaType<QHash<QString, QStringList>>();
    QSignalSpy updated(&mBackend, &SingleSqliteBackend::updated);
    QVERIFY(mBackend.deleteIncidence(mNotebook->uid(), event));
    QCOMPARE(updated.count(), 1);
    // Can't mark a non-existing event as deleted
    // This is emitting a warning.
    QVERIFY(!mBackend.deleteIncidence(mNotebook->uid(), event2));
    QCOMPARE(updated.count(), 1);

    incidences.clear();
    QVERIFY(mBackend.incidences(&incidences, mNotebook->uid(), event.uid()));
    QVERIFY(incidences.isEmpty());

    incidences.clear();
    QVERIFY(mBackend.deletedIncidences(&incidences, mNotebook->uid()));
    QCOMPARE(incidences.count(), 1);
    QCOMPARE(event.uid(), incidences[0]->uid());

    QVERIFY(mBackend.purgeDeletedIncidences(mNotebook->uid(), incidences));
    incidences.clear();
    QVERIFY(mBackend.deletedIncidences(&incidences, mNotebook->uid()));
    QVERIFY(incidences.isEmpty());

    QList<QVariant> args = updated.takeFirst();
    QCOMPARE(args.count(), 3);
    const QHash<QString, QStringList> added = args[0].value<QHash<QString, QStringList>>();
    const QHash<QString, QStringList> modified = args[1].value<QHash<QString, QStringList>>();
    const QHash<QString, QStringList> deleted = args[2].value<QHash<QString, QStringList>>();
    QVERIFY(added.isEmpty());
    QVERIFY(modified.isEmpty());
    QCOMPARE(deleted.count(), 1);
    QVERIFY(deleted.contains(mNotebook->uid()));
    QCOMPARE(deleted.value(mNotebook->uid())[0], event.instanceIdentifier());
}

void tst_backend::testPurgeIncidence()
{
    Event event, event2;

    // Add an event.
    QVERIFY(mBackend.addIncidence(mNotebook->uid(), event));

    Incidence::List incidences;
    QVERIFY(mBackend.incidences(&incidences, mNotebook->uid(), event.uid()));
    QCOMPARE(incidences.count(), 1);
    QCOMPARE(event.uid(), incidences[0]->uid());

    // Purge an event
    qRegisterMetaType<QHash<QString, QStringList>>();
    QSignalSpy updated(&mBackend, &SingleSqliteBackend::updated);
    QVERIFY(mBackend.purgeIncidence(mNotebook->uid(), event));
    QCOMPARE(updated.count(), 1);
    // Purge a non-existing event is a no-op
    QVERIFY(mBackend.purgeIncidence(mNotebook->uid(), event2));
    QCOMPARE(updated.count(), 2);

    incidences.clear();
    QVERIFY(mBackend.deletedIncidences(&incidences, mNotebook->uid()));
    QVERIFY(incidences.isEmpty());
    incidences.clear();
    QVERIFY(mBackend.incidences(&incidences, mNotebook->uid(), event.uid()));
    QVERIFY(incidences.isEmpty());

    QList<QVariant> args = updated.takeFirst();
    QCOMPARE(args.count(), 3);
    const QHash<QString, QStringList> added = args[0].value<QHash<QString, QStringList>>();
    const QHash<QString, QStringList> modified = args[1].value<QHash<QString, QStringList>>();
    const QHash<QString, QStringList> deleted = args[2].value<QHash<QString, QStringList>>();
    QVERIFY(added.isEmpty());
    QVERIFY(modified.isEmpty());
    QCOMPARE(deleted.count(), 1);
    QVERIFY(deleted.contains(mNotebook->uid()));
    QCOMPARE(deleted.value(mNotebook->uid())[0], event.instanceIdentifier());
}

void tst_backend::testPurgeOnAddIncidence()
{
    Event event;

    // Add an event and delete it.
    QVERIFY(mBackend.addIncidence(mNotebook->uid(), event));
    QVERIFY(mBackend.deleteIncidence(mNotebook->uid(), event));

    Incidence::List incidences;
    QVERIFY(mBackend.deletedIncidences(&incidences, mNotebook->uid()));
    QCOMPARE(incidences.count(), 1);
    QCOMPARE(event.uid(), incidences[0]->uid());

    // Re-add it.
    QVERIFY(mBackend.addIncidence(mNotebook->uid(), event));
    incidences.clear();
    QVERIFY(mBackend.incidences(&incidences, mNotebook->uid(), event.uid()));
    QCOMPARE(incidences.count(), 1);
    QCOMPARE(event.uid(), incidences[0]->uid());
    incidences.clear();
    QVERIFY(mBackend.deletedIncidences(&incidences, mNotebook->uid()));
    QVERIFY(incidences.isEmpty());

    // Cleanup
    QVERIFY(mBackend.purgeIncidence(mNotebook->uid(), event));
}

void tst_backend::testDeferSaveIncidence()
{
    Event event;

    QVERIFY(mBackend.deferSaving());

    // Add an event.
    qRegisterMetaType<QHash<QString, QStringList>>();
    QSignalSpy updated(&mBackend, &SingleSqliteBackend::updated);
    QVERIFY(mBackend.addIncidence(mNotebook->uid(), event));
    QCOMPARE(updated.count(), 0);

    QVERIFY(mBackend.commit());
    QCOMPARE(updated.count(), 1);

    Incidence::List incidences;
    QVERIFY(mBackend.incidences(&incidences, mNotebook->uid(), event.uid()));
    QCOMPARE(incidences.count(), 1);
    QCOMPARE(event.uid(), incidences[0]->uid());

    const QList<QVariant> args = updated.takeFirst();
    QCOMPARE(args.count(), 3);
    const QHash<QString, QStringList> added = args[0].value<QHash<QString, QStringList>>();
    const QHash<QString, QStringList> modified = args[1].value<QHash<QString, QStringList>>();
    const QHash<QString, QStringList> deleted = args[2].value<QHash<QString, QStringList>>();
    QCOMPARE(added.count(), 1);
    QVERIFY(modified.isEmpty());
    QVERIFY(deleted.isEmpty());
    QVERIFY(added.contains(mNotebook->uid()));
    QCOMPARE(added.value(mNotebook->uid())[0], event.instanceIdentifier());

    // Cleanup
    QVERIFY(mBackend.purgeIncidence(mNotebook->uid(), event));
}

void tst_backend::testSingleNotebookFetch()
{
    Event event, event2, event3, event4;

    event.setDtStart(QDateTime(QDate(2023, 5, 5), QTime(16, 26)));
    event.recurrence()->setDaily(1);
    Incidence::Ptr exception(event.clone());
    QVERIFY(exception);
    exception->clearRecurrence();
    exception->setRecurrenceId(event.dtStart().addDays(2));
    exception->setDtStart(exception->recurrenceId().addSecs(3600));

    QVERIFY(mBackend.deferSaving());
    QVERIFY(mBackend.addIncidence(mNotebook->uid(), event));
    QVERIFY(mBackend.addIncidence(mNotebook->uid(), *exception));
    QVERIFY(mBackend.addIncidence(mNotebook->uid(), event2));
    QVERIFY(mBackend.addIncidence(mNotebook->uid(), event3));
    QVERIFY(mBackend.addIncidence(mNotebook->uid(), event4));
    QVERIFY(mBackend.commit());

    Incidence::List list;
    // Cannot load from any notebook.
    // This is emitting a warning.
    QVERIFY(!mBackend.incidences(&list, QString()));
    QVERIFY(list.isEmpty());
    QVERIFY(mBackend.incidences(&list, QString::fromLatin1("Not a notebook UID")));
    QVERIFY(list.isEmpty());
    QVERIFY(mBackend.incidences(&list, mNotebook->uid()));
    QCOMPARE(list.count(), 5);
    list.clear();
    QVERIFY(mBackend.incidences(&list, mNotebook->uid(), event.uid()));
    QCOMPARE(list.count(), 2);
    list.clear();
    QVERIFY(mBackend.incidences(&list, mNotebook->uid(), event2.uid()));
    QCOMPARE(list.count(), 1);

    QVERIFY(mBackend.deferSaving());
    QVERIFY(mBackend.deleteIncidence(mNotebook->uid(), event3));
    QVERIFY(mBackend.deleteIncidence(mNotebook->uid(), event4));
    QVERIFY(mBackend.commit());

    list.clear();
    QVERIFY(mBackend.deletedIncidences(&list, mNotebook->uid()));
    QCOMPARE(list.count(), 2);

    // Cleanup
    QVERIFY(mBackend.deferSaving());
    QVERIFY(mBackend.purgeIncidence(mNotebook->uid(), event));
    QVERIFY(mBackend.purgeIncidence(mNotebook->uid(), *exception));
    QVERIFY(mBackend.purgeIncidence(mNotebook->uid(), event2));
    QVERIFY(mBackend.purgeIncidence(mNotebook->uid(), event3));
    QVERIFY(mBackend.purgeIncidence(mNotebook->uid(), event4));
    QVERIFY(mBackend.commit());
}

void tst_backend::testMultiNotebookFetch()
{
    Notebook notebook(QString::fromLatin1("Test multi"), QString());
    QVERIFY(mBackend.addNotebook(notebook, false));

    Event event, event2, event3, event4;

    event.setDtStart(QDateTime(QDate(2023, 5, 5), QTime(16, 26), QTimeZone::systemTimeZone()));
    event.setDtEnd(event.dtStart().addSecs(1800));
    event.setSummary(QString::fromLatin1("Test summary with string 'azertyu'\\ fooplop"));
    event.recurrence()->setDaily(1);
    Incidence::Ptr exception(event.clone());
    QVERIFY(exception);
    exception->clearRecurrence();
    exception->setRecurrenceId(event.dtStart().addDays(2));
    exception->setDtStart(exception->recurrenceId().addSecs(3600));
    exception.staticCast<Event>()->setDtEnd(exception->dtStart().addSecs(1800));
    exception->setSummary(QString::fromLatin1("Test exception with string 'azerty_'\\ %plop"));

    event2.setSummary(QString::fromLatin1("Test summary with string 'azerty_'\\ %plop"));
    event2.setDtStart(QDateTime(QDate(2023, 5, 10), QTime(15, 12), QTimeZone::systemTimeZone()));
    event2.setDtEnd(event2.dtStart().addSecs(1800));
    // event3.setUid(event.uid()); // Currently not supported by the DB
    event3.setDescription(QString::fromLatin1("Test description with string 'azerty_'\\ %plop"));
    event3.setDtStart(QDateTime(QDate(2023, 5, 10), QTime(16, 26), QTimeZone::systemTimeZone()));
    event3.setDtEnd(event3.dtStart().addSecs(1800));
    event4.setLocation(QString::fromLatin1("Test location with string 'azerty_'\\ %plop"));
    event4.setDtStart(QDateTime(QDate(2023, 5, 10), QTime()));
    event4.setAllDay(true);

    QVERIFY(mBackend.deferSaving());
    QVERIFY(mBackend.addIncidence(mNotebook->uid(), event));
    QVERIFY(mBackend.addIncidence(mNotebook->uid(), *exception));
    QVERIFY(mBackend.addIncidence(mNotebook->uid(), event2));
    QVERIFY(mBackend.addIncidence(notebook.uid(), event3));
    QVERIFY(mBackend.addIncidence(notebook.uid(), event4));
    QVERIFY(mBackend.commit());

    QHash<QString, Incidence::List> hash;
    QVERIFY(mBackend.incidences(&hash, QString()));
    QCOMPARE(hash.count(), 2);
    QVERIFY(hash.contains(mNotebook->uid()));
    QVERIFY(hash.contains(notebook.uid()));
    QCOMPARE(hash.value(mNotebook->uid()).count(), 3);
    QCOMPARE(hash.value(notebook.uid()).count(), 2);

    hash.clear();
    QVERIFY(mBackend.incidences(&hash, event.uid()));
    QCOMPARE(hash.count(), 1); // Should be 2 later
    QVERIFY(hash.contains(mNotebook->uid()));
    // QVERIFY(hash.contains(notebook.uid())); // should contain it later
    QCOMPARE(hash.value(mNotebook->uid()).count(), 2);
    // QCOMPARE(hash.value(notebook.uid()).count(), 1);

    hash.clear();
    QHash<QString, QStringList> identifiers;
    QVERIFY(mBackend.search(&hash, &identifiers, QString::fromLatin1("rTy_'\\ %p")));
    // Return exact matching occurrences.
    QCOMPARE(identifiers.count(), 2);
    QVERIFY(identifiers.contains(mNotebook->uid()));
    QVERIFY(identifiers.contains(notebook.uid()));
    QStringList ids = identifiers.value(mNotebook->uid());
    QCOMPARE(ids.count(), 2);
    QVERIFY(ids.contains(event2.instanceIdentifier()));
    QVERIFY(ids.contains(exception->instanceIdentifier()));
    ids = identifiers.value(notebook.uid());
    QCOMPARE(ids.count(), 2);
    QVERIFY(ids.contains(event3.instanceIdentifier()));
    QVERIFY(ids.contains(event4.instanceIdentifier()));
    // Load all matching incidences, including non matching parents.
    QCOMPARE(hash.count(), 2);
    QVERIFY(hash.contains(mNotebook->uid()));
    QVERIFY(hash.contains(notebook.uid()));
    QCOMPARE(hash.value(mNotebook->uid()).count(), 3);
    QCOMPARE(hash.value(notebook.uid()).count(), 2);

    hash.clear();
    // Without including recurring events.
    QVERIFY(mBackend.incidences(&hash, QDateTime(QDate(2023, 5, 7), QTime(17, 26), QTimeZone::systemTimeZone()),
                                QDateTime(QDate(2023, 5, 10), QTime(16, 27), QTimeZone::systemTimeZone()), false));
    QCOMPARE(hash.count(), 2);
    QVERIFY(hash.contains(mNotebook->uid()));
    QVERIFY(hash.contains(notebook.uid()));
    QCOMPARE(hash.value(mNotebook->uid()).count(), 1);
    QCOMPARE(hash.value(notebook.uid()).count(), 2);

    hash.clear();
    // Including out-of-range recurring events.
    QVERIFY(mBackend.incidences(&hash, QDateTime(QDate(2023, 5, 7), QTime(17, 26), QTimeZone::systemTimeZone()),
                                QDateTime(QDate(2023, 5, 10), QTime(16, 27), QTimeZone::systemTimeZone()), true));
    QCOMPARE(hash.count(), 2);
    QVERIFY(hash.contains(mNotebook->uid()));
    QVERIFY(hash.contains(notebook.uid()));
    QCOMPARE(hash.value(mNotebook->uid()).count(), 3);
    QCOMPARE(hash.value(notebook.uid()).count(), 2);

    // Cleanup
    QVERIFY(mBackend.deferSaving());
    QVERIFY(mBackend.purgeIncidence(mNotebook->uid(), event));
    QVERIFY(mBackend.purgeIncidence(mNotebook->uid(), *exception));
    QVERIFY(mBackend.purgeIncidence(mNotebook->uid(), event2));
    QVERIFY(mBackend.commit());
    QVERIFY(mBackend.deleteNotebook(notebook));
}

void tst_backend::testPurgeOnNotebookDeletion()
{
    Notebook notebook(QString::fromLatin1("Test purge notebook"), QString());
    QVERIFY(mBackend.addNotebook(notebook, false));

    Event event, event2, event3, event4;

    QVERIFY(mBackend.deferSaving());
    QVERIFY(mBackend.addIncidence(notebook.uid(), event));
    QVERIFY(mBackend.addIncidence(notebook.uid(), event2));
    QVERIFY(mBackend.addIncidence(notebook.uid(), event3));
    QVERIFY(mBackend.addIncidence(notebook.uid(), event4));
    QVERIFY(mBackend.commit());

    QVERIFY(mBackend.deferSaving());
    QVERIFY(mBackend.deleteIncidence(notebook.uid(), event3));
    QVERIFY(mBackend.deleteIncidence(notebook.uid(), event4));
    QVERIFY(mBackend.commit());

    Incidence::List list;
    QVERIFY(mBackend.deletedIncidences(&list, notebook.uid()));
    QCOMPARE(list.count(), 2);
    list.clear();
    QVERIFY(mBackend.incidences(&list, notebook.uid()));
    QCOMPARE(list.count(), 2);

    QVERIFY(mBackend.deleteNotebook(notebook));

    list.clear();
    QVERIFY(mBackend.deletedIncidences(&list, notebook.uid()));
    QVERIFY(list.isEmpty());
    list.clear();
    QVERIFY(mBackend.incidences(&list, notebook.uid()));
    QVERIFY(list.isEmpty());
}

#include "tst_backend.moc"
QTEST_MAIN(tst_backend)

/*
  This file is part of the mkcal library.

  Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
  Contact: Alvaro Manera <alvaro.manera@nokia.com>

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

#ifdef MKCAL_DIRECTORY_SUPPORT

/**
  @file
  This file is part of the API for handling calendar data and
  defines the DirectoryStorage class.

  @brief
  This class provides a calendar storage as a local file.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
*/

#include <config-mkcal.h>
#include "directorystorage.h"

#include <qtlockedfile.h>

#include <exceptions.h>
#include <icalformat.h>
#include <calendar.h>
#include <vcalformat.h>
using namespace KCalCore;

#include <kdebug.h>

#include <QtCore/QDir>
#include <QtCore/QFileSystemWatcher>

#if defined(HAVE_UUID_UUID_H)
#include <uuid/uuid.h>
#endif

using namespace mKCal;

/**
  Private class that helps to provide binary compatibility between releases.
*/
//@cond PRIVATE
class mKCal::DirectoryStorage::Private
{
  public:
    Private( const QString &directory, CalFormat *format )
      : mDirectory( directory ),
        mFormat( format ),
        mLf( 0 ),
        mIsOpened ( false ),
        mIsLoading ( false ),
        mWatcher( 0 )
    {}
    ~Private() { if (mFormat) delete mFormat; }

    QStringList open( const QDir &dir );
    bool load( const Calendar::Ptr &calendar, QFile *file, bool deleted,
               const QString &notebook );
    void close();
    bool check( const QFile &file );
    bool lockNotebooks( QtLockedFile::LockMode mode );
    void unlockNotebooks();
    QHash<QString,Notebook::Ptr>loadNotebooks();
    bool saveNotebooks( QHash<QString,Notebook::Ptr> hash );

    QString mDirectory;
    CalFormat *mFormat;
    QtLockedFile *mLf;
    QList<QString> mNotebooksToLoad;
    QList<QString> mNotebooksToSave;
    QHash<QString, QFileInfo> mFileInfos;
    QHash<QString, QString> mUidMappings;
    bool mIsOpened;
    bool mIsLoading;
    QFileSystemWatcher *mWatcher;
    QHash<QString,Notebook::Ptr> mNotebooks; // name to notebook
};
//@endcond

DirectoryStorage::DirectoryStorage( const ExtendedCalendar::Ptr &cal, const QString &directory,
                                    CalFormat *format, bool validateNotebooks )
  : ExtendedStorage( cal, validateNotebooks ),
    d( new Private( directory, format ) )
{
  cal->registerObserver( this );
}

DirectoryStorage::~DirectoryStorage()
{
  close();
  calendar()->unregisterObserver( this );
  delete d;
}

QString DirectoryStorage::directory() const
{
  return d->mDirectory;
}

void DirectoryStorage::setFormat( CalFormat *format )
{
  if ( d->mFormat ) {
    delete d->mFormat;
  }
  d->mFormat = format;
}

CalFormat *DirectoryStorage::format() const
{
  return d->mFormat;
}

bool DirectoryStorage::snapshot( const QString &from, const QString &to )
{
  kDebug() << "snapshot from" << from << "to" << to;

  QDir fromDir( from );
  QDir toDir( to );
  if ( !fromDir.exists() || ( !toDir.exists() && !toDir.mkpath( toDir.path() ) ) ) {
    kError() << "cannot snapshot" << from << to;
    return false;
  }
  // Use directory lock for getting consistent data across notebooks.
  // Now incidences cannot be moved between notebooks at loading time.
  QString lockFile = from + "/." + fromDir.dirName();
  QtLockedFile lf(lockFile);
  if ( !lf.open( QFile::ReadOnly ) ) {
    kError() << "cannot open" << from;
    return false;
  }
  if ( !lf.lock( QtLockedFile::ReadLock ) ) {
    lf.close();
    kError() << "cannot lock" << from;
    return false;
  }
  bool success = false;
  QFileInfoList infoList = fromDir.entryInfoList( QDir::Files );
  for ( int i = 0; i < infoList.size(); ++i ) {
    success = false;
    QString notebook = infoList.at(i).fileName();
    QString fromFile = from + '/' + notebook;
    QtLockedFile ff(fromFile);
    if ( ff.open( QFile::ReadOnly ) ) {
      if ( ff.lock( QtLockedFile::ReadLock ) ) {
        QString toFile = to + '/' + notebook;
        QtLockedFile tf( toFile );
        if ( tf.open( QFile::WriteOnly ) ) {
          if ( tf.lock( QtLockedFile::WriteLock ) ) {
            QByteArray data = ff.readAll();
            if ( tf.write( data ) == data.size() ) {
              kDebug() << "snapshotted" << toFile;
              QString deletedFromFile = from + "/." + notebook;
              QFile dff( deletedFromFile );
              if ( dff.open( QFile::ReadOnly ) ) {
                QString deletedToFile = to + "/." + notebook;
                QFile dtf( deletedToFile );
                if ( dtf.open( QFile::WriteOnly ) ) {
                  QByteArray data = dff.readAll();
                  if ( dtf.write( data ) == data.size() ) {
                    kDebug() << "snapshotted" << deletedToFile;
                    success = true;
                  }
                  dtf.close();
                }
                dff.close();
              }
            }
            tf.unlock();
          }
          tf.close();
        }
        ff.unlock();
      }
      ff.close();
    }
    if ( !success ) {
      kError() << "snapshot failed" << fromFile;
      break;
    }
  }
  lf.unlock();
  lf.close();

  return success;
}

bool DirectoryStorage::open()
{
  if ( !d->mIsOpened && !d->mDirectory.isEmpty() ) {
    QDir dir( d->mDirectory );
    if ( !dir.exists() && !dir.mkpath( dir.path() ) ) {
      kError() << "cannot use" << d->mDirectory;
      return false;
    }
    // Write lock notebooks to make sure notebooks file is there
    d->mLf = new QtLockedFile( d->mDirectory + "/." + dir.dirName() );
    if ( !loadNotebooks() ) {
      kError() << "cannot load notebooks from" << d->mDirectory;
      return false;
    }
    if ( d->lockNotebooks( QtLockedFile::WriteLock ) ) {
      QStringList paths = d->open( dir );
      paths << d->mDirectory;
      d->mWatcher = new QFileSystemWatcher( paths );
      if ( d->mWatcher ) {
        connect( d->mWatcher, SIGNAL(fileChanged(const QString &)),
                 this, SLOT(fileChanged(const QString &)) );
        connect( d->mWatcher, SIGNAL(directoryChanged(const QString &)),
                 this, SLOT(directoryChanged(const QString &)) );
        d->mIsOpened = true;
      }
      d->unlockNotebooks();
    }
    if ( !d->mIsOpened ) {
      d->close();
    }
  }
  return d->mIsOpened;
}

QStringList DirectoryStorage::Private::open( const QDir &dir )
{
  QStringList paths;

  kDebug() << "scanning" << dir.dirName();

  QFileInfoList infolist = dir.entryInfoList( QDir::Files );
  for ( int i = 0; i < infolist.size(); ++i ) {
    QString notebook = infolist.at( i ).fileName();
    if ( !mNotebooksToLoad.contains( notebook ) ) {
      mNotebooksToLoad.append( notebook );
      if ( !mFileInfos.contains( notebook ) ) {
        kDebug() << "found" << notebook;
      }
    }
    paths << mDirectory + '/' + notebook;
  }
  return paths;
}

bool DirectoryStorage::load()
{
  if ( !d->mIsOpened || d->mDirectory.isEmpty() ) {
    return false;
  }

  // Use directory lock for getting consistent data across notebooks.
  // Now incidences cannot be moved between notebooks at loading time.
  if ( !d->lockNotebooks( QtLockedFile::ReadLock ) ) {
    return false;
  }

  bool success = true;

  while ( !d->mNotebooksToLoad.isEmpty() ) {
    QString notebook = d->mNotebooksToLoad.first();
    success &= load( notebook );
  }
  calendar()->setModified( false );

  d->unlockNotebooks();

  return success;
}

bool DirectoryStorage::load( const QString &notebook )
{
  if ( !d->mIsOpened || d->mDirectory.isEmpty() || notebook.isEmpty() ) {
    return false;
  }

  if ( d->mNotebooksToLoad.contains( notebook ) ) {
    // Remove anyway to avoid continuous failures with broken notebooks.
    d->mNotebooksToLoad.removeOne( notebook );

    if ( validateNotebooks() && !d->mNotebooks.contains( notebook ) ) {
      kWarning() << "not loading invalidated notebook" << notebook;
      return true;
    }
    kDebug() << "loading" << notebook;

    // Get all incidences, lock the file first.
    QString fileName = d->mDirectory + '/' + notebook;
    QtLockedFile lf( fileName );
    if ( !lf.open( QFile::ReadOnly ) ) {
      kError() << "cannot open" << fileName;
      return false;
    }
    if ( !lf.lock( QtLockedFile::ReadLock ) ) {
      lf.close();
      kError() << "cannot lock" << fileName;
      return false;
    }

    bool success = d->load( calendar(), &lf, false, notebook );

    // Get deleted incidences, no need to lock this separately.
    QString deletedFileName = d->mDirectory + "/." + notebook;
    QFile f( deletedFileName );
    if ( f.open( QFile::ReadOnly ) ) {
      d->load( calendar(), &f, true, notebook );
      f.close();
    }
    QFileInfo info( lf );
    d->mFileInfos.insert( notebook, info );
    d->mWatcher->removePath( fileName );
    d->mWatcher->addPath( fileName );

    lf.unlock();
    lf.close();

    return success;
  }
  return true;
}

bool DirectoryStorage::Private::load( const Calendar::Ptr &calendar, QFile *file,
                                      bool deleted, const QString &notebook )
{
  QTextStream ts( file );
  ts.setCodec( "ISO 8859-1" );
  QByteArray text = ts.readAll().trimmed().toLatin1();

  mIsLoading = true;

  bool success = mFormat && mFormat->fromRawString( calendar, text, deleted, notebook );
  if ( !success ) {
    ICalFormat iCal;
    success = iCal.fromRawString( calendar, text, deleted, notebook );
    if ( !success ) {
      if ( iCal.exception() &&
           iCal.exception()->code() == Exception::CalVersion1 ) {
        VCalFormat vCal;
        success = vCal.fromRawString( calendar, text, deleted, notebook );
      }
    }
    if ( !success ) {
      kError() << "Loading failed for " << notebook
               << ( iCal.exception() ?
                    QString( iCal.exception()->code() ) : QLatin1String( "No exception" ) );
    }
  }
  mIsLoading = false;

  return success;
}

bool DirectoryStorage::Private::check( const QFile &file )
{
  QFileInfo current( file );

  bool changed = current.size() != 0; // false if doesn't exist or is created

  if ( mFileInfos.contains( current.fileName() ) ) {
    QFileInfo stored = mFileInfos.value( current.fileName() );
    if ( current.lastModified() == stored.lastModified() ) {
      changed = false;
    }
  }
  return changed;
}

bool DirectoryStorage::save()
{
  if ( !d->mIsOpened || d->mDirectory.isEmpty() ) {
    return false;
  }

  // Use directory lock for saving consistent data between notebooks.
  if ( !d->lockNotebooks( QtLockedFile::WriteLock ) ) {
    return false;
  }

  bool success = true;

  while ( !d->mNotebooksToSave.isEmpty() ) {
    QString notebook = d->mNotebooksToSave.first();
    success &= save(notebook);
  }
  calendar()->setModified( false );

  d->unlockNotebooks();

  return success;
}

bool DirectoryStorage::save( const QString &notebook )
{
  if ( !d->mIsOpened || d->mDirectory.isEmpty() || notebook.isEmpty() ) {
    return false;
  }

  kDebug() << "saving" << notebook;

  if ( d->mNotebooksToSave.contains( notebook ) ) {
    // Remove always to avoid continuous failures with broken notebooks.
    d->mNotebooksToSave.removeOne( notebook );

    if ( !isValidNotebook( notebook ) ) {
      kDebug() << "invalid notebook - not saving";
      return true;
    }

    QString fileName = d->mDirectory + '/' + notebook;
    QtLockedFile lf( fileName );
    if ( !lf.open( QFile::ReadWrite ) ) {
      kError() << "cannot open" << fileName;
      return false;
    }
    if ( !lf.lock( QtLockedFile::WriteLock ) ) {
      kError() << "cannot lock" << fileName;
      return false;
    }
    bool success = true;
    QString reason;

    if ( d->mNotebooksToLoad.contains( notebook ) || d->check( lf ) ) {
      // Load before saving.
      d->mNotebooksToLoad.removeOne( notebook );
      kDebug() << notebook << "has been changed, loading first";
      success = d->load( calendar(), &lf, false, notebook );

      // Get deleted incidences, no need to lock this separately.
      QString deletedFileName = d->mDirectory + "/." + notebook;
      QFile f( deletedFileName );
      if ( f.open( QFile::ReadOnly ) ) {
        d->load( calendar(), &f, true, notebook );
        f.close();
      }
      if ( !success ) {
        // Just go on with the save in this case, the file might
        // be empty or mungled. Overwrite anyway, we have the lock.
        kWarning() << "failed to read" << notebook << "before saving";
      }
    }
    lf.resize( 0 );

    CalFormat *format = d->mFormat ? d->mFormat : new ICalFormat;
    // Put all incidences.
    QString text = format->toString( calendar(), notebook );
    success = !text.isEmpty();
    if ( success ) {
      QByteArray textUtf8 = text.toUtf8();
      success = lf.write( textUtf8.data(), textUtf8.size() );
    } else if ( format->exception() ) {
// PENDING(kdab) Review
#ifdef KDAB_TEMPORARILY_REMOVED
      reason = format->exception()->message();
#else
      reason = "some reason";
#endif
    }
    if (success) {
      // Put deleted incidences.
      QString fileName = d->mDirectory + "/." + notebook;
      QFile f( fileName );
      if ( f.open( QFile::ReadWrite | QFile::Truncate ) ) {
        text = format->toString( calendar(), notebook, true );
        if ( !text.isEmpty() ) {
          QByteArray textUtf8 = text.toUtf8();
          f.write( textUtf8.data(), textUtf8.size() );
        }
        f.close();
      }
    }
    if ( !d->mFormat ) {
      delete format;
    }

    lf.flush();

    QFileInfo info( lf );
    d->mFileInfos.insert( notebook, info );
    d->mWatcher->removePath( fileName );
    d->mWatcher->addPath( fileName );

    // Reset all alarms.
    clearAlarms( notebook );
    Incidence::List values = calendar()->incidences( notebook );
    Incidence::List::const_iterator it;
    for ( it = values.begin(); it != values.end(); ++it ) {
      resetAlarms( *it );
    }

    lf.unlock();
    lf.close();

    if ( !success ) {
      kError() << "saving failed for" << notebook << reason;
    }

    return success;
  }
  return true;
}

bool DirectoryStorage::close()
{
  if ( d->mIsOpened ) {
    d->close();
    d->mIsOpened = false;
    return true;
  } else {
    return false;
  }
}

void DirectoryStorage::Private::close()
{
  if ( mWatcher ) {
    delete mWatcher;
    mWatcher = 0;
  }

  if ( mLf ) {
    delete mLf;
    mLf = 0;
  }
}

void DirectoryStorage::calendarModified( bool modified, Calendar *calendar )
{
  Q_UNUSED( modified );
  Q_UNUSED( calendar );
  //kDebug() << "calendarModified called" << modified;
}

void DirectoryStorage::calendarIncidenceAdded( const Incidence::Ptr &incidence )
{
#if defined(HAVE_UUID_UUID_H)
  uuid_t uuid;
  char suuid[64];
  QString uid = incidence->uid();

  if ( uuid_parse( incidence->uid().toLatin1().data(), uuid ) ) {
    // Cannot accept this id, create better one.
    if ( !d->mUidMappings.contains( uid ) ) {
      uuid_generate_random( uuid );
      uuid_unparse( uuid, suuid );
      incidence->setUid( QString( suuid ) );
      kDebug() << "changing" << uid << "to" << incidence->uid();
    } else {
      // This is a child to a recurrent event.
      incidence->setUid( d->mUidMappings.value( uid ) );
      kDebug() << "mapping" << uid << "to" << incidence->uid();
    }
  }
  if ( !uid.isEmpty() ) {
    d->mUidMappings.insert( uid, incidence->uid() );
  }
  if ( !d->mIsLoading ) {
    kDebug() << "created incidence" << incidence->uid();
  }
#else
//KDAB_TODO:
#ifdef __GNUC__
#warning no uuid support. what to do now?
#endif
#endif
}

void DirectoryStorage::calendarIncidenceChanged( const Incidence::Ptr &incidence )
{
  if ( !d->mIsLoading ) {
    QString notebook = calendar()->notebook( incidence );
    if ( !notebook.isEmpty() && !d->mNotebooksToSave.contains( notebook ) ) {
      d->mNotebooksToSave.append( notebook );
    }

    d->mUidMappings.insert( incidence->uid(), incidence->uid() );

    kDebug() << "updating incidence" << incidence->uid() << "in" << notebook;
  }
}

void DirectoryStorage::calendarIncidenceDeleted( const Incidence::Ptr &incidence )
{
  QString notebook = calendar()->notebook( incidence );
  if ( !notebook.isEmpty() ) {
    // this will call calendarIncidenceChanged
    calendar()->setNotebook( incidence, QString() );
  }
  kDebug() << "deleting incidence" << incidence->uid() << "from" << notebook;
}

void DirectoryStorage::calendarIncidenceAdditionCanceled( const Incidence::Ptr &incidence )
{
  Q_UNUSED( incidence );
}

void DirectoryStorage::fileChanged ( const QString &path )
{
  QFileInfo info( path );
  if ( !d->mNotebooksToLoad.contains( info.fileName() ) ) {
    d->mNotebooksToLoad.append( info.fileName() );
  }

  setModified( info.fileName() );

  kDebug() << info.fileName() << "has been modified";
}

void DirectoryStorage::directoryChanged ( const QString &path )
{
  QFileInfo info( path );

  QDir dir( path );
  QStringList paths = d->open( dir );
  d->mWatcher->removePaths( d->mWatcher->files() );
  d->mWatcher->addPaths( paths );

  loadNotebooks();
}

bool DirectoryStorage::insertedIncidences( Incidence::List *list, const KDateTime &after,
                                           const QString &notebook )
{
  Incidence::List values = calendar()->incidences( notebook );
  Incidence::List::const_iterator it;

  for ( it = values.begin(); it != values.end(); ++it ) {
    if ( !after.isValid() || (*it)->created() > after ) {
      Incidence::Ptr incidence = Incidence::Ptr( (*it)->clone() );
      list->append( incidence );
    }
  }
  return true;
}

bool DirectoryStorage::modifiedIncidences( Incidence::List *list, const KDateTime &after,
                                           const QString &notebook )
{
  Incidence::List values = calendar()->incidences( notebook );
  Incidence::List::const_iterator it;

  for ( it = values.begin(); it != values.end(); ++it ) {
    if ( !after.isValid() || (*it)->lastModified() > after ) {
      Incidence::Ptr incidence = Incidence::Ptr( (*it)->clone() );
      list->append( incidence );
    }
  }
  return true;
}

bool DirectoryStorage::deletedIncidences( Incidence::List *list,
                                          const KDateTime &after, const QString &notebook )
{
  Event::List events = calendar()->deletedEvents();
  Event::List::Iterator eit;
  for ( eit = events.begin(); eit != events.end(); ++eit ) {
    if ( notebook.isEmpty() || notebook == calendar()->notebook(*eit) ) {
      if ( !after.isValid() || (*eit)->lastModified() > after ) {
        Incidence::Ptr incidence = Incidence::Ptr( (*eit)->clone() );
        list->append( incidence );
      }
    }
  }

  Todo::List todos = calendar()->deletedTodos();
  Todo::List::Iterator tit;
  for ( tit = todos.begin(); tit != todos.end(); ++tit ) {
    if ( notebook.isEmpty() || notebook == calendar()->notebook( *eit ) ) {
      if ( !after.isValid() || (*tit)->lastModified() > after ) {
        Incidence::Ptr incidence = Incidence::Ptr( (*tit)->clone() );
        list->append( incidence );
      }
    }
  }

  Journal::List journals = calendar()->deletedJournals();
  Journal::List::Iterator jit;
  for ( jit = journals.begin(); jit != journals.end(); ++jit ) {
    if ( notebook.isEmpty() || notebook == calendar()->notebook( *eit ) ) {
      if ( !after.isValid() || (*jit)->lastModified() > after ) {
        Incidence::Ptr incidence = Incidence::Ptr( (*jit)->clone() );
        list->append( incidence );
      }
    }
  }
  return true;
}

bool DirectoryStorage::allIncidences( Incidence::List *list, const QString &notebook )
{
  Incidence::List values = calendar()->incidences(notebook);
  Incidence::List::const_iterator it;

  for ( it = values.begin(); it != values.end(); ++it ) {
    Incidence::Ptr incidence = Incidence::Ptr( (*it)->clone() );
    list->append( incidence );
  }

  return true;
}

bool DirectoryStorage::duplicateIncidences( Incidence::List *list, const Incidence::Ptr &incidence,
                                            const QString &notebook )
{
  Incidence::List values = calendar()->incidences( notebook );
  Incidence::List::const_iterator it;

  for ( it = values.begin(); it != values.end(); ++it ) {
    if ( ( ( incidence->dtStart() == (*it)->dtStart() ) ||
           ( !incidence->dtStart().isValid() && !(*it)->dtStart().isValid() ) ) &&
         ( incidence->summary() == (*it)->summary() ) ) {
      Incidence::Ptr incidence = Incidence::Ptr( (*it)->clone() );
      list->append( incidence );
    }
  }
  return true;
}

KDateTime DirectoryStorage::incidenceDeletedDate( const Incidence::Ptr &incidence )
{
  Q_UNUSED( incidence );
  return KDateTime();
}

int DirectoryStorage::eventCount()
{
  return 0;
}

int DirectoryStorage::todoCount()
{
  return 0;
}

int DirectoryStorage::journalCount()
{
  return 0;
}

bool DirectoryStorage::notifyOpened( const Incidence::Ptr &incidence )
{
  Q_UNUSED( incidence );
  return false;
}

// Conformance to ExtendedStorage //

bool DirectoryStorage::load( const QString &uid, const KDateTime &recurrenceId )
{
  Q_UNUSED( uid );
  Q_UNUSED( recurrenceId );

  return load();
}

bool DirectoryStorage::load( const QDate &date )
{
  Q_UNUSED( date );

  return load();
}

bool DirectoryStorage::load( const QDate &start, const QDate &end )
{
  Q_UNUSED( start );
  Q_UNUSED( end );

  return load();
}

bool DirectoryStorage::loadNotebookIncidences( const QString &notebookUid )
{
  Q_UNUSED( notebookUid );

  return load();
}

bool DirectoryStorage::loadJournals()
{
    return load();
}

int DirectoryStorage::loadJournals( int limit, KDateTime *last )
{
    Q_UNUSED(limit);
    *last = KDateTime();
    load();
    return 0;
}

bool DirectoryStorage::loadPlainIncidences()
{
  return load();
}

bool DirectoryStorage::loadRecurringIncidences()
{
  return load();
}

bool DirectoryStorage::loadGeoIncidences()
{
  return load();
}

bool DirectoryStorage::loadGeoIncidences( float geoLatitude, float geoLongitude,
                                          float diffLatitude, float diffLongitude )
{
  Q_UNUSED( geoLatitude );
  Q_UNUSED( geoLongitude );
  Q_UNUSED( diffLatitude );
  Q_UNUSED( diffLongitude );

  return load();
}

bool DirectoryStorage::loadAttendeeIncidences()
{
  return load();
}

int DirectoryStorage::loadUncompletedTodos()
{
  return (int)load();
}

int DirectoryStorage::loadCompletedTodos( bool hasDate, int limit, KDateTime *last )
{
  Q_UNUSED( hasDate );
  Q_UNUSED( limit );
  Q_UNUSED( last );

  return (int)load();
}

int DirectoryStorage::loadIncidences( bool hasDate, int limit, KDateTime *last )
{
  Q_UNUSED( hasDate );
  Q_UNUSED( limit );
  Q_UNUSED( last );

  return (int)load();
}

int DirectoryStorage::loadFutureIncidences( int limit, KDateTime *last )
{
  Q_UNUSED( limit );
  Q_UNUSED( last );

  return (int)load();
}

int DirectoryStorage::loadGeoIncidences( bool hasDate, int limit, KDateTime *last )
{
  Q_UNUSED(hasDate);
  Q_UNUSED(limit);
  Q_UNUSED(last);

  return (int)load();
}

int DirectoryStorage::loadUnreadInvitationIncidences()
{
  return (int)load();
}

int DirectoryStorage::loadOldInvitationIncidences( int limit, KDateTime *last )
{
  Q_UNUSED(limit);
  Q_UNUSED(last);

  return (int)load();
}

Person::List DirectoryStorage::loadContacts()
{
  Person::List list;
  return list;
}

int DirectoryStorage::loadContactIncidences( const Person::Ptr &person, int limit, KDateTime *last )
{
  Q_UNUSED( person );
  Q_UNUSED( limit );
  Q_UNUSED( last );

  return (int)load();
}

bool DirectoryStorage::cancel()
{
  return true;
}

bool DirectoryStorage::Private::lockNotebooks( QtLockedFile::LockMode mode )
{
  if ( !mLf ) {
    return false;
  }

  if ( mode == QtLockedFile::ReadLock ) {
    if ( !mLf->open( QFile::ReadOnly ) ) {
      kError() << "cannot open" << mDirectory;
      return false;
    }
  } else if ( mode == QtLockedFile::WriteLock ) {
    if ( !mLf->open( QFile::ReadWrite ) ) {
      kError() << "cannot open" << mDirectory;
      return false;
    }
  } else {
    return false;
  }

  if ( !mLf->lock( mode ) ) {
    mLf->close();
    kError() << "cannot lock" << mDirectory;
    return false;
  }
  return true;
}

void DirectoryStorage::Private::unlockNotebooks()
{
  if ( mLf ) {
    mLf->unlock();
    mLf->close();
  }
}

bool DirectoryStorage::loadNotebooks()
{
  d->mIsLoading = true;

  d->lockNotebooks( QtLockedFile::ReadLock );

  // hash = all notebooks in storage
  // mNotebooks = all allowed notebooks
  QHash<QString,Notebook::Ptr> hash = d->loadNotebooks();
  Notebook::List list = hash.values();
  Notebook::List::Iterator it = list.begin();
  for ( ; it != list.end(); ++it ) {
    Notebook::Ptr nb = (*it);
    if ( !addNotebook( nb ) ) {
      kWarning() << "cannot add notebook" << nb->uid() << nb->name() << "to storage";
    } else {
      d->mNotebooks.insert( nb->name(), nb );
    }
  }
  d->unlockNotebooks();

  d->mIsLoading = false;

  return true;
}

bool DirectoryStorage::reloadNotebooks()
{
  return true;
}

bool DirectoryStorage::modifyNotebook( const Notebook::Ptr &nb, DBOperation dbop, bool signal )
{
  Q_UNUSED( signal );
  // hash = all notebooks in storage
  // mNotebooks = all allowed notebooks
  if ( !d->mIsLoading ) {
    d->lockNotebooks( QtLockedFile::WriteLock );
    QHash<QString,Notebook::Ptr> hash = d->loadNotebooks();
    if ( hash.contains( nb->name() ) ) {
      Notebook::Ptr old = hash.value( nb->name() );
      if ( dbop == DBInsert || dbop == DBUpdate ) {
        old = nb;
      } else if ( dbop == DBDelete ) {
        hash.remove( nb->name() );
      }
    } else if ( dbop == DBInsert || dbop == DBUpdate ) {
      Notebook::Ptr copy = Notebook::Ptr( new Notebook( *nb ) );
      hash.insert( copy->name(), copy );
    }
    d->saveNotebooks( hash );
    d->unlockNotebooks();
  }
  if ( dbop == DBInsert || dbop == DBUpdate ) {
    d->mNotebooks.insert( nb->name(), nb );
  } else if ( dbop == DBDelete ) {
    d->mNotebooks.remove( nb->name() );
  }
  return true;
}

QHash<QString,Notebook::Ptr> DirectoryStorage::Private::loadNotebooks()
{
  QHash<QString,Notebook::Ptr> hash;

  QTextStream stream( mLf );

  for ( QString line = stream.readLine(); !line.isNull(); line = stream.readLine() ) {
    QStringList npv = line.split( ':' );
    if ( npv.size() == 3 ) {
      QString name = npv.at( 0 );
      QString parameter = npv.at( 1 );
      QString value = npv.at( 2 );

      if ( !hash.contains( name ) ) {
        hash.insert( name, Notebook::Ptr( new Notebook() ) );
        kDebug() << "parsing" << name;
      }
      Notebook::Ptr notebook = hash.value(name);
      if ( notebook ) {
        if ( parameter == "uid" ) {
          notebook->setUid( value );
        } else if ( parameter == "name" ) {
          notebook->setName( value );
        } else if ( parameter == "description" ) {
          notebook->setDescription( value );
        } else if ( parameter == "color" ) {
          notebook->setColor( value );
        } else if ( parameter == "isShared" ) {
          notebook->setIsShared( value == "true" ? true : false );
        } else if ( parameter == "isMaster" ) {
          notebook->setIsMaster( value == "true" ? true : false );
        } else if ( parameter == "isOviSync" ) {
          notebook->setIsSynchronized( value == "true" ? true : false );
        } else if ( parameter == "isReadOnly" ) {
          notebook->setIsReadOnly( value == "true" ? true : false );
        } else if ( parameter == "isVisible" ) {
          notebook->setIsVisible( value == "true" ? true : false );
        } else if ( parameter == "isRunTimeOnly" ) {
          notebook->setRunTimeOnly( value == "true" ? true : false );
        } else if ( parameter == "flags" ) {
          notebook->setFlags( value.toInt() );
        } else if ( parameter == "syncDate" ) {
          notebook->setSyncDate( KDateTime::fromString( value ) );
        } else if ( parameter == "pluginName" ) {
          notebook->setPluginName( value );
        } else if ( parameter == "account" ) {
          notebook->setAccount( value );
        } else if ( parameter == "attachmentSize" ) {
          notebook->setAttachmentSize( value.toInt() );
        } else if ( parameter == "modifiedDate" ) {
          notebook->setModifiedDate( KDateTime::fromString( value ) );
        } else if ( parameter == "isDefault" ) {
          notebook->setIsDefault( value == "true" ? true : false );
        } else {
          kWarning() << "invalid parameter" << parameter << value;
        }
      }
    }
  }
  return hash;
}

bool DirectoryStorage::Private::saveNotebooks( QHash<QString,Notebook::Ptr> hash )
{
  QTextStream stream( mLf );
  mLf->resize( 0 );

  Notebook::List list = hash.values();
  Notebook::List::Iterator it = list.begin();
  for ( ; it != list.end(); ++it ) {
    Notebook::Ptr nb = *it;
    stream << nb->name() << ":uid:" << nb->uid() << "\n";
    stream << nb->name() << ":name:" << nb->name() << "\n";
    stream << nb->name() << ":description:" << nb->description() << "\n";
    stream << nb->name() << ":color:" << nb->color() << "\n";
    stream << nb->name() << ":flags:" << nb->flags()  << "\n";
    stream << nb->name() << ":syncDate:" << nb->syncDate().toString() << "\n";
    stream << nb->name() << ":pluginName:" << nb->pluginName() << "\n";
    stream << nb->name() << ":account:" << nb->account() << "\n";
    stream << nb->name() << ":attachmentSize:" << nb->attachmentSize() << "\n";
    stream << nb->name() << ":modifiedDate:" << nb->modifiedDate().toString() << "\n";
  }
  return true;
}


void DirectoryStorage::virtual_hook( int id, void *data )
{
  Q_UNUSED( id );
  Q_UNUSED( data );
  Q_ASSERT( false );
}

#endif

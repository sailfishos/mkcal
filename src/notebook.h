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
/**
  @file
  This file is part of the API for handling calendar data and
  defines the Notebook class.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
*/

#ifndef MKCAL_NOTEBOOK_H
#define MKCAL_NOTEBOOK_H

#include "mkcal_export.h"

#include <KCalendarCore/Incidence>

#include <QtCore/QList>

namespace mKCal {

/**
  @brief
  Placeholder for Notebook parameters.
*/
class MKCAL_EXPORT Notebook
{
public:
    /**
      A shared pointer to a Notebook object.
    */
    typedef QSharedPointer<Notebook> Ptr;

    /**
      A shared pointer to a non-mutable Notebook.
    */
    typedef QSharedPointer<const Notebook> ConstPtr;

    /**
      List of notebooks.
    */
    typedef QList<Ptr> List;

    /**
      Constructs a new Notebook object.
    */
    explicit Notebook();

    explicit Notebook(const QString &name, const QString &description,
                      const QString &color = {});

    explicit Notebook(const QString &uid, const QString &name,
                      const QString &description, const QString &color,
                      bool isShared, bool isMaster, bool oviSync,
                      bool isReadOnly, bool isVisible);

    explicit Notebook(const QString &uid, const QString &name,
                      const QString &description, const QString &color,
                      bool isShared, bool isMaster, bool isSynchronized,
                      bool isReadOnly, bool isVisible, const QString &pluginName,
                      const QString &account, int attachmentSize);

    /**
      Constructs an Notebook as a copy of another Notebook object.
      @param n is the Notebook to copy.
    */
    Notebook(const Notebook &n);

    /**
      Destructor.
    */
    virtual ~Notebook();

    /**
      Returns if the notebook is valid or null constructed.
    */
    bool isValid() const;

    /**
      Returns the uid of the notebook.
      @see setUid().
    */
    QString uid() const;

    /**
      Set the uid of the notebook.
      Typically called internally by the storage.
      @param uid unique identifier.
    */
    void setUid(const QString &uid);

    /**
      Returns the name of the notebook.

      @see setName()
    */
    QString name() const;

    /**
      Set the name of the notebook.

      @param name notebook name
    */
    void setName(const QString &name);

    /**
      Returns the notebook description.
      @see setDescription().
    */
    QString description() const;

    /**
      Set the description of the notebook.
      @param description notebook description.
    */
    void setDescription(const QString &description);

    /**
      Returns the notebook color in the form of #RRGGBB.
      @see setColor().
    */
    QString color() const;

    /**
      Set notebook color.
      @param color notebook color.
    */
    void setColor(const QString &color);

    /**
      Returns true if notebook is shared.
      @see setIsShared().
    */
    bool isShared() const;

    /**
      Set notebook sharing.
      The actual meaning is storage specific.
      @param isShared true to allow sharing.
    */
    void setIsShared(bool isShared);

    /**
      Returns true if notebook is a master.
      @see setIsMaster().
    */
    bool isMaster() const;

    /**
      Set notebook master status.
      The actual meaning is storage specific.

      @param isMaster true to set master status
    */
    void setIsMaster(bool isMaster);

    /**
      Returns true if notebook is synchronized to OVI.
      @see setIsOviSync().
    */
    bool isSynchronized() const;

    /**
      Set notebook OVI sync.
      The actual meaning is storage specific.
      @param oviSync true to set OVI sync.
    */
    void setIsSynchronized(bool oviSync);

    /**
      Returns true if notebook is read-only.
      @see setIsReadOnly().
    */
    bool isReadOnly() const;

    /**
      Set notebook into read-only mode.
      This means that storages will not save any notes for the notebook.
      Typically used for showing shared notebooks without write permission.

      @param isReadOnly true to set read-only mode
    */
    void setIsReadOnly(bool isReadOnly);

    /**
      Returns true if notebook is visible.
      @see setIsVisible().
    */
    bool isVisible() const;

    /**
      Set notebook visibility.
      Calendar will check this value for including/excluding incidences
      into search lists.
      @param isVisible true to set visible mode.
    */
    void setIsVisible(bool isVisible);

    /**
      Returns true if the notebook is never going to be saved; false otherwise.
      @see setRunTimeOnly().
    */
    bool isRunTimeOnly() const;

    /**
      Determines if the notebook is only in memory and won't be saved into
      any storage.
      @param isRunTime true if the incidence is never going to be saved.
    */
    void setRunTimeOnly(bool isRunTime);

    /**
      Returns sync date.
      @see setSyncDate().
    */
    QDateTime syncDate() const;

    /**
      Sets sync date of notebook.
      Used internally by storages and synchronization services.
      @param syncDate last sync date.
      @see syncDate().
    */
    void setSyncDate(const QDateTime &syncDate);

    /**
      Gets the name of the plugin that created the notebook.
      @return The name of the plugin that owns the notebook.
      @see setPluginName().
    */
    QString pluginName() const;

    /**
      Sets the plugin name that created the notebook (if any).
      @param pluginName The name of the plugin.
      @see pluginName();
      */
    void setPluginName(const QString &pluginName);

    /**
      Gets the account associated with the notebook.
      @return The account.
      @see setAccount().
    */
    QString account() const;

    /**
      Sets the account associated with the notebook (if any).
      @param account The account
      @see account().
      */
    void setAccount(const QString &account);

    /**
      Gets the maximum size of attachments allowed in the notebook.
      @return The size in bytes.
      @see setAttachmentSize().
    */
    int attachmentSize() const;

    /**
      Sets the size of attachments allowed in the notebook (if any).
      @param size The size in bytes. 0 means no attachments allowed; -1 means unlimited size.
      @see attachmentSize().
      */
    void setAttachmentSize(int size);

    /**
      Returns modification date, in UTC.
      Every time a property is set, the time is updated.
    */
    QDateTime modifiedDate() const;

    /**
      Sets modified date of notebook.
      Used internally by storages and synchronization services.
      @param modifiedDate last modification date.
      @see modifiedDate().
    */
    void setModifiedDate(const QDateTime &modifiedDate);

    /**
      Returns creation date, in UTC.
    */
    QDateTime creationDate() const;

    /**
      Sets creation date of notebook.
      Used internally.
      @param date creation date.
      @see creationDate().
    */
    void setCreationDate(const QDateTime &date);

    /**
      Returns true if notebook is shareable.
      @see setIsShareable().
    */
    bool isShareable() const;

    /**
      Set notebook to shareable.
      @param isShareable true to set shareable.
    */
    void setIsShareable(bool isShareable);

    /**
      Gets the people shared with the notebook.
      @return list of people shared with.
      @see setSharedWith().
    */
    QStringList sharedWith() const;

    /**
      Gets the people shared with the notebook.
      @return string of people shared with.
      @see setSharedWithStr().
    */
    QString sharedWithStr() const;

    /**
      Sets the people shared with the notebook.
      @param sharedWith The list of people shared with
      @see sharedWith().
    */
    void setSharedWith(const QStringList &sharedWith);

    /**
      Sets the people shared with the notebook.
      @param sharedWith The string of people shared with.
      @see sharedWithStr().
    */
    void setSharedWithStr(const QString &sharedWith);

    /**
      Gets the sync profile of the notebook.

       @return string sync profile.

       @see setSyncProfile()
    */
    QString syncProfile() const;

    /**
      Sets the sync profile of  the notebook.

      @param syncProfile string of sync profile

      @see syncProfile();
      */
    void setSyncProfile(const QString &syncProfile);

    /**
      Set whether the events are allowed to this notebook or not. By
      default, this is true.
    */
    void setEventsAllowed(bool eventsAllowed);

    /**
      Accessor querying whether events are allowed in this notebook.
    */
    bool eventsAllowed() const;

    /**
      Set whether the journals are allowed to this notebook or not.
      By default, this is true.
    */
    void setJournalsAllowed(bool journalsAllowed);

    /**
      Accessor querying whether journals are allowed in this notebook.
    */
    bool journalsAllowed() const;

    /**
      Set whether the todos are allowed to this notebook or not.
      By default, this is true.
    */
    void setTodosAllowed(bool todosAllowed);

    /**
      Accessor querying whether todos are allowed in this notebook.
    */
    bool todosAllowed() const;

    /**
      Utility function to find out whether the incidence is allowed
      within this notebook or not.
    */
    bool incidenceAllowed(KCalendarCore::Incidence::Ptr incidence) const;

    /**
       Set a key/value property. Setting the value to the empty string
       will remove the property.
       @param key The name of the property.
       @param value The value of the property.
    */
    void setCustomProperty(const QByteArray &key, const QString &value);

    /**
       A getter function for a custom property, see
       setCustomProperty().
       @param key The name of the property.
       @param default A default value if the property does not exists.
    */
    QString customProperty(const QByteArray &key, const QString &defaultValue = QString()) const;

    /**
       List the keys of all stored custom properties.
    */
    QList<QByteArray> customPropertyKeys() const;

    /**
      Assignment operator.
     */
    Notebook &operator=(const Notebook &other);

    /**
      Compare this with notebook for equality.
    */
    bool operator==(const Notebook &notebook) const;

private:
    //@cond PRIVATE
    class Private;
    Private *const d;
    //@endcond
};

}

#endif

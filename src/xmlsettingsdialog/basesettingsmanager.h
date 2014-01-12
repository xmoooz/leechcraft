/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2014  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#ifndef XMLSETTINGSDIALOG_BASESETTINGSMANAGER_H
#define XMLSETTINGSDIALOG_BASESETTINGSMANAGER_H
#include <memory>
#include <QMap>
#include <QPair>
#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QDynamicPropertyChangeEvent>
#include <QPointer>
#include "xsdconfig.h"

#define PROP2CHAR(a) (a.toUtf8 ().constData ())

typedef std::shared_ptr<QSettings> Settings_ptr;

namespace LeechCraft
{
class SettingsThread;

namespace Util
{
	/** @brief Base class for settings manager.
	 *
	 * Facilitates creation of settings managers due to providing some
	 * frequently used features.
	 */
	class XMLSETTINGSMANAGER_API BaseSettingsManager : public QObject
	{
		Q_OBJECT

		typedef QPair<QPointer<QObject>, QByteArray> ObjectElement_t;
		typedef QMultiMap<QByteArray, ObjectElement_t> Properties2Object_t;
		Properties2Object_t ApplyProps_;
		Properties2Object_t SelectProps_;

		bool IsInitializing_;
		bool CleanupScheduled_;

		friend class LeechCraft::SettingsThread;
	protected:
		bool ReadAllKeys_;
	public:
		BaseSettingsManager (bool readAllKeys = false, QObject* = 0);

		/** @brief Initalizes the settings manager.
		 *
		 * Loads all settings from the QSettings created by BeginSettings and
		 * creates dynamic properties for them.
		 *
		 * @sa Release
		 */
		void Init ();

		/** @brief Prepares the settings manager for deletion.
		 *
		 * Flushes all settigns to the QSettings created by BeginSettings
		 * to prepare settings manager object for the deletion.
		 *
		 * @sa Init
		 */
		void Release ();

		enum EventFlag
		{
			Apply = 0x01,
			Select = 0x02
		};
		Q_DECLARE_FLAGS (EventFlags, EventFlag)

		/** @brief Subscribes object to property changes.
		 *
		 * After a property has changed, a specified function of the
		 * object is called to notify it about the change.
		 *
		 * @param[in] propName The name of property object wants to
		 * subscribe to.
		 * @param[in] object The object instance that will get
		 * notifications.
		 * @param[in] funcName Name of the function that will be called.
		 * Note that it should be known to the Qt's metaobject system, so
		 * it should be a (public) slot.
		 */
		void RegisterObject (const QByteArray& propName,
				QObject* object, const QByteArray& funcName, EventFlags flags = EventFlag::Apply);

		/** @brief Subscribes object to property changes.
		 *
		 * This is an overloaded function provided for convenience.
		 *
		 * @param[in] propNames The names of properties object wants to
		 * subscribe to.
		 * @param[in] object The object instance that will get
		 * notifications.
		 * @param[in] funcName Name of the function that will be called.
		 * Note that it should be known to the Qt's metaobject system, so
		 * it should be a (public) slot.
		 */
		void RegisterObject (const QList<QByteArray>& propNames,
				QObject* object, const QByteArray& funcName, EventFlags flags = EventFlag::Apply);

		/** @brief Gets a property with default value.
		 *
		 * This is a wrapper around standard QObject::property() function.
		 * It checks whether specified property exists, and if so, it
		 * returns its value, otherwise it creates this property, sets its
		 * value to def and returns def.
		 *
		 * @param[in] propName Name of the property that should be checked
		 * and returned.
		 * @param[in] def Default value of the property.
		 * @return Resulting value of the property.
		 */
		QVariant Property (const QString& propName, const QVariant& def);

		/** @brief Sets the value directly, without metaproperties system.
		 *
		 * This function just plainly calls setValue() on the
		 * corresponding QSettings object, without all this
		 * properties machinery.
		 *
		 * @param[in] path The key path.
		 * @param[in] val The value to set.
		 *
		 * @sa GetRawValue()
		 */
		void SetRawValue (const QString& path, const QVariant& val);

		/** @brief Gets the value that is set directly.
		 *
		 * This function plainly returns the value that is set
		 * previously with SetRawValue().
		 *
		 * @param[in] path The key path.
		 * @param[in] def The default value to return.
		 * @return The stored value.
		 *
		 * @sa SetRawValue()
		 */
		QVariant GetRawValue (const QString& path, const QVariant& def = QVariant ()) const;

		void OptionSelected (const QByteArray&, const QVariant&);

		std::shared_ptr<void> EnterInitMode ();
	protected:
		virtual bool event (QEvent*);

		/*! @brief Allocates and returns a QSettings object suitable for
		 * use.
		 *
		 * @return The created QSettings object.
		 * @sa EndSettings
		 */
		virtual QSettings* BeginSettings () const = 0;

		/*! @brief Correctly closes the QSettings object.
		 *
		 * Closes the QSettings object previously created by
		 * BeginSettings. It should NOT delete it, BaseSettignsManager's
		 * code would do that.
		 *
		 * @param[in] settings The QSettings object.
		 * @sa BeginSettings
		 */
		virtual void EndSettings (QSettings *settings) const = 0;

		virtual void PropertyChanged (const QString&, const QVariant&);

		virtual Settings_ptr GetSettings () const;
	private slots:
		void scheduleCleanup ();
		void cleanupObjects ();
	};
}
}

#endif

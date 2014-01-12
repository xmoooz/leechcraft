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

#pragma once

#include <QtPlugin>
#include <QVariant>
#include <QStringList>
#include <boost/variant/variant.hpp>

namespace LeechCraft
{
	/** @brief A single additional AdvancedNotifications field.
	 *
	 * This data structure describes an additional field in the
	 * AdvancedNotifications notification entities. The field ID (the
	 * name of the corresponding key in LeechCraft::Entity::Additional_
	 * map) is stored in the ID_ member.
	 *
	 * This structure also carries information about field name, type,
	 * description and such.
	 */
	struct ANFieldData
	{
		/** @brief The field ID.
		 *
		 * The field ID is the value of the corresponding key in the
		 * LeechCraft::Entity::Additional_ map.
		 */
		QString ID_;

		/** @brief The name of the field.
		 *
		 * This member contains the human-readable name of this field.
		 */
		QString Name_;

		/** @brief The description of the field.
		 *
		 * This member contains the human-readable description of this
		 * field.
		 */
		QString Description_;

		/** @brief The type of this field.
		 *
		 * This member contains the type of the value of this field -
		 * the value for the corresponding key (equal to ID_) in the
		 * LeechCraft::Entity::Additional_ map.
		 *
		 * For now only QVariant::Int, QVariant::String and
		 * QVariant::StringList are supported.
		 */
		QVariant::Type Type_;

		/** @brief The types of the event that contain this field.
		 *
		 * This member contains the types of the events that contain
		 * this field. This field won't be checked in events of types
		 * not mentioned here.
		 */
		QStringList EventTypes_;

		/** @brief The allowed values of this field.
		 *
		 * If this list is non-empty, only values from this list are
		 * allowed.
		 *
		 * This currently only makes sense for QVariant::String and
		 * QVariant::StringList, in which case each QVariant in this list
		 * should be a QString.
		 */
		QVariantList AllowedValues_;

		/** @brief Constructs an empty field info.
		 *
		 * The corresponding type is invalid, and all other members are
		 * empty.
		 */
		ANFieldData ()
		: Type_ (QVariant::Invalid)
		{
		}

		/** @brief Constructs field with the given info variables.
		 *
		 * @param[in] id The ID of the field.
		 * @param[in] name The name of the field.
		 * @param[in] description The description of the field.
		 * @param[in] type The type of the field.
		 * @param[in] events The list of events for this field.
		 * @param[in] values The allowed values of this field.
		 */
		ANFieldData (const QString& id,
				const QString& name,
				const QString& description,
				QVariant::Type type,
				const QStringList& events,
				const QVariantList& values = {})
		: ID_ (id)
		, Name_ (name)
		, Description_ (description)
		, Type_ (type)
		, EventTypes_ (events)
		, AllowedValues_ (values)
		{
		}
	};

	/** @brief Describes a field with integer values.
	 */
	struct ANIntFieldValue
	{
		/** @brief The boundary of the field.
		 */
		int Boundary_;

		/** @brief Describes the elementary semantics of Boundary_.
		 */
		enum Operation
		{
			/** @brief The value should be greater than Boundary_.
			 */
			OGreater = 0x01,

			/** @brief The value should be less than Boundary_.
			 */
			OLess = 0x02,

			/** @brief The value should be equal to Boundary_.
			 */
			OEqual = 0x04
		};

		Q_DECLARE_FLAGS (Operations, Operation)

		/** @brief Describe the semantics of Boundary_.
		 *
		 * This is the combination of values in Operation enum.
		 */
		Operations Ops_;
	};

	/** @brief Describes a field with QString values.
	 */
	struct ANStringFieldValue
	{
		/** @brief The regular expression the values should (not) match.
		 */
		QRegExp Rx_;

		/** @brief Whether the values should match or not match Rx_.
		 *
		 * If this is true, the values should match Rx_, and shouldn't
		 * otherwise.
		 */
		bool Contains_;
	};

	/** @brief A combination of all possible descriptions.
	 */
	typedef boost::variant<ANIntFieldValue, ANStringFieldValue> ANFieldValue;
}

/** @brief Interface for plugins emitting AdvancedNotifications entries.
 *
 * This interface should be implemented by plugins that support the
 * AdvancedNotifications framework, emit the corresponding entities and
 * provide additional fields in those entities.
 *
 * The list of additional fields is described by the list of
 * corresponding structures returned from the GetANFields() member.
 *
 * If a plugin doesn't define any additional fields, it may choose to
 * not implement this interface.
 *
 * @sa LeechCraft::ANFieldData
 */
class Q_DECL_EXPORT IANEmitter
{
public:
	virtual ~IANEmitter () {}

	/** @brief Returns the list of additional fields.
	 *
	 * This function returns the list of additional fields and their
	 * semantics that may be present in the notifications emitted by
	 * this plugin.
	 *
	 * This list must not change during single run session.
	 *
	 * Please refer to the documentation of the LeechCraft::ANFieldData
	 * structure for more information.
	 *
	 * @return The list of additional AdvancedNotifications fields.
	 *
	 * @sa LeechCraft::ANFieldData
	 */
	virtual QList<LeechCraft::ANFieldData> GetANFields () const = 0;
};

Q_DECLARE_INTERFACE (IANEmitter, "org.Deviant.LeechCraft.IANEmitter/1.0");
Q_DECLARE_METATYPE (LeechCraft::ANFieldData);
Q_DECLARE_METATYPE (LeechCraft::ANFieldValue);
Q_DECLARE_METATYPE (QList<LeechCraft::ANFieldValue>);

Q_DECLARE_OPERATORS_FOR_FLAGS (LeechCraft::ANIntFieldValue::Operations);

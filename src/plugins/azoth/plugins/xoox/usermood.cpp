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

#include "usermood.h"
#include <algorithm>
#include <QDomElement>
#include <QtDebug>
#include <QXmppElement.h>

namespace LeechCraft
{
namespace Azoth
{
namespace Xoox
{
	const QString NsMoodNode = "http://jabber.org/protocol/mood";

	static const char* MoodStr[] =
	{
		"afraid",
		"amazed",
		"amorous",
		"angry",
		"annoyed",
		"anxious",
		"aroused",
		"ashamed",
		"bored",
		"brave",
		"calm",
		"cautious",
		"cold",
		"confident",
		"confused",
		"contemplative",
		"contented",
		"cranky",
		"crazy",
		"creative",
		"curious",
		"dejected",
		"depressed",
		"disappointed",
		"disgusted",
		"dismayed",
		"distracted",
		"embarrassed",
		"envious",
		"excited",
		"flirtatious",
		"frustrated",
		"grateful",
		"grieving",
		"grumpy",
		"guilty",
		"happy",
		"hopeful",
		"hot",
		"humbled",
		"humiliated",
		"hungry",
		"hurt",
		"impressed",
		"in_awe",
		"in_love",
		"indignant",
		"interested",
		"intoxicated",
		"invincible",
		"jealous",
		"lonely",
		"lost",
		"lucky",
		"mean",
		"moody",
		"nervous",
		"neutral",
		"offended",
		"outraged",
		"playful",
		"proud",
		"relaxed",
		"relieved",
		"remorseful",
		"restless",
		"sad",
		"sarcastic",
		"satisfied",
		"serious",
		"shocked",
		"shy",
		"sick",
		"sleepy",
		"spontaneous",
		"stressed",
		"strong",
		"surprised",
		"thankful",
		"thirsty",
		"tired",
		"undefined",
		"weak",
		"worried"
	};

	QString UserMood::GetNodeString ()
	{
		return NsMoodNode;
	}

	QXmppElement UserMood::ToXML () const
	{
		QXmppElement mood;
		mood.setTagName ("mood");
		mood.setAttribute ("xmlns", NsMoodNode);

		if (Mood_ != MoodEmpty)
		{
			QXmppElement elem;
			elem.setTagName (MoodStr [Mood_]);
			mood.appendChild (elem);

			if (!Text_.isEmpty ())
			{
				QXmppElement text;
				text.setTagName ("text");
				text.setValue (Text_);
				mood.appendChild (text);
			}
		}

		QXmppElement result;
		result.setTagName ("item");
		result.appendChild (mood);
		return result;
	}

	void UserMood::Parse (const QDomElement& elem)
	{
		Mood_ = MoodEmpty;
		Text_.clear ();

		QDomElement moodElem = elem.firstChildElement ("mood");
		if (moodElem.namespaceURI () != NsMoodNode)
			return;

		QDomElement mood = moodElem.firstChildElement ();
		while (!mood.isNull ())
		{
			const QString& tagName = mood.tagName ();
			if (tagName == "text")
				Text_ = mood.text ();
			else
				SetMoodStr (tagName);

			mood = mood.nextSiblingElement ();
		}
	}

	QString UserMood::Node () const
	{
		return GetNodeString ();
	}

	PEPEventBase* UserMood::Clone () const
	{
		return new UserMood (*this);
	}

	UserMood::Mood UserMood::GetMood () const
	{
		return Mood_;
	}

	void UserMood::SetMood (UserMood::Mood mood)
	{
		Mood_ = mood;
	}

	QString UserMood::GetMoodStr () const
	{
		return Mood_ == MoodEmpty ?
				QString () :
				MoodStr [Mood_];
	}

	void UserMood::SetMoodStr (const QString& str)
	{
		if (str.isEmpty ())
		{
			Mood_ = MoodEmpty;
			return;
		}

		const auto pos = std::find (std::begin (MoodStr), std::end (MoodStr), str);
		if (pos == std::end (MoodStr))
		{
			qWarning () << Q_FUNC_INFO
					<< str
					<< "not found";
			Mood_ = MoodEmpty;
		}
		else
			Mood_ = static_cast<Mood> (std::distance (std::begin (MoodStr), pos));
	}

	QString UserMood::GetText () const
	{
		return Text_;
	}

	void UserMood::SetText (const QString& text)
	{
		Text_ = text;
	}
}
}
}

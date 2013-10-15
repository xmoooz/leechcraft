/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
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

#include "sourceobject.h"
#include <memory>
#include <QtDebug>
#include <QTimer>
#include <QTextCodec>
#include <gst/gst.h>

#ifdef WITH_LIBGUESS
extern "C"
{
#include <libguess/libguess.h>
}
#endif

#include "audiosource.h"
#include "path.h"
#include "../core.h"
#include "../xmlsettingsmanager.h"

namespace LeechCraft
{
namespace LMP
{
	namespace
	{
		gboolean CbBus (GstBus *bus, GstMessage *message, gpointer data)
		{
			auto src = static_cast<SourceObject*> (data);

			switch (GST_MESSAGE_TYPE (message))
			{
			case GST_MESSAGE_ERROR:
				src->HandleErrorMsg (message);
				break;
			case GST_MESSAGE_TAG:
				src->HandleTagMsg (message);
				break;
			case GST_MESSAGE_NEW_CLOCK:
			case GST_MESSAGE_ASYNC_DONE:
				break;
			case GST_MESSAGE_BUFFERING:
				src->HandleBufferingMsg (message);
				break;
			case GST_MESSAGE_STATE_CHANGED:
				src->HandleStateChangeMsg (message);
				break;
			case GST_MESSAGE_DURATION:
				QTimer::singleShot (0,
						src,
						SLOT (updateTotalTime ()));
				break;
			case GST_MESSAGE_ELEMENT:
				src->HandleElementMsg (message);
				break;
			case GST_MESSAGE_EOS:
				src->HandleEosMsg (message);
				break;
			default:
				qDebug () << Q_FUNC_INFO << GST_MESSAGE_TYPE (message);
				break;
			}

			return true;
		}

		gboolean CbAboutToFinish (GstElement*, gpointer data)
		{
			static_cast<SourceObject*> (data)->HandleAboutToFinish ();
			return true;
		}

		gboolean CbSourceChanged (GstElement*, GParamSpec*, gpointer data)
		{
			static_cast<SourceObject*> (data)->SetupSource ();
			return true;
		}
	}

	SourceObject::SourceObject (QObject *parent)
	: QObject (parent)
#if GST_VERSION_MAJOR < 1
	, Dec_ (gst_element_factory_make ("playbin2", "play"))
#else
	, Dec_ (gst_element_factory_make ("playbin", "play"))
#endif
	, Path_ (nullptr)
	, IsSeeking_ (false)
	, LastCurrentTime_ (-1)
	, PrevSoupRank_ (0)
	, OldState_ (SourceState::Stopped)
	{
		auto bus = gst_pipeline_get_bus (GST_PIPELINE (Dec_));
		gst_bus_add_watch (bus, CbBus, this);
		gst_object_unref (bus);

		g_signal_connect (Dec_, "about-to-finish", G_CALLBACK (CbAboutToFinish), this);
		g_signal_connect (Dec_, "notify::source", G_CALLBACK (CbSourceChanged), this);

		qRegisterMetaType<AudioSource> ("AudioSource");

		auto timer = new QTimer (this);
		connect (timer,
				SIGNAL (timeout ()),
				this,
				SLOT (handleTick ()));
		timer->start (1000);
	}

	SourceObject::~SourceObject ()
	{
	}

	bool SourceObject::IsSeekable () const
	{
		std::shared_ptr<GstQuery> query (gst_query_new_seeking (GST_FORMAT_TIME), gst_query_unref);

		if (!gst_element_query (GST_ELEMENT (Dec_), query.get ()))
			return false;

		gboolean seekable = false;
		GstFormat format;
		gint64 start = 0, stop = 0;
		gst_query_parse_seeking (query.get (), &format, &seekable, &start, &stop);
		return seekable;
	}

	SourceState SourceObject::GetState () const
	{
		return OldState_;
	}

	QString SourceObject::GetErrorString () const
	{
		return {};
	}

	QString SourceObject::GetMetadata (Metadata field) const
	{
		switch (field)
		{
		case Metadata::Artist:
			return Metadata_ ["artist"];
		case Metadata::Album:
			return Metadata_ ["album"];
		case Metadata::Title:
			return Metadata_ ["title"];
		case Metadata::Genre:
			return Metadata_ ["genre"];
		case Metadata::Tracknumber:
			return Metadata_ ["tracknumber"];
		case Metadata::NominalBitrate:
			return Metadata_ ["bitrate"];
		case Metadata::MinBitrate:
			return Metadata_ ["minimum-bitrate"];
		case Metadata::MaxBitrate:
			return Metadata_ ["maximum-bitrate"];
		}

		qWarning () << Q_FUNC_INFO
				<< "unknown field"
				<< static_cast<int> (field);

		return {};
	}

	qint64 SourceObject::GetCurrentTime ()
	{
		if (GetState () != SourceState::Paused)
		{
			auto format = GST_FORMAT_TIME;
			gint64 position = 0;
#if GST_VERSION_MAJOR >= 1
			gst_element_query_position (GST_ELEMENT (Dec_), format, &position);
#else
			gst_element_query_position (GST_ELEMENT (Dec_), &format, &position);
#endif
			LastCurrentTime_ = position;
		}
		return LastCurrentTime_ / GST_MSECOND;
	}

	qint64 SourceObject::GetRemainingTime () const
	{
		auto format = GST_FORMAT_TIME;
		gint64 duration = 0;
#if GST_VERSION_MAJOR >= 1
		if (!gst_element_query_duration (GST_ELEMENT (Dec_), format, &duration))
#else
		if (!gst_element_query_duration (GST_ELEMENT (Dec_), &format, &duration))
#endif
			return -1;

		return (duration - LastCurrentTime_) / GST_MSECOND;
	}

	qint64 SourceObject::GetTotalTime () const
	{
		auto format = GST_FORMAT_TIME;
		gint64 duration = 0;
#if GST_VERSION_MAJOR >= 1
		if (gst_element_query_duration (GST_ELEMENT (Dec_), format, &duration))
#else
		if (gst_element_query_duration (GST_ELEMENT (Dec_), &format, &duration))
#endif
			return duration / GST_MSECOND;
		return -1;
	}

	void SourceObject::Seek (qint64 pos)
	{
		if (!IsSeekable ())
			return;

		if (OldState_ == SourceState::Playing)
			IsSeeking_ = true;

		gst_element_seek (GST_ELEMENT (Dec_), 1.0, GST_FORMAT_TIME,
				GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, pos * GST_MSECOND,
				GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
	}

	void SourceObject::SetTransitionTime (int time)
	{
// 		Obj_->setTransitionTime (time);
	}

	AudioSource SourceObject::GetCurrentSource () const
	{
		return CurrentSource_;
	}

	namespace
	{
		uint SetSoupRank (uint rank)
		{
			const auto factory = gst_element_factory_find ("souphttpsrc");
			if (!factory)
			{
				qWarning () << Q_FUNC_INFO
						<< "cannot find soup factory";
				return 0;
			}

			const auto oldRank = gst_plugin_feature_get_rank (GST_PLUGIN_FEATURE (factory));
			gst_plugin_feature_set_rank (GST_PLUGIN_FEATURE (factory), rank);
#if GST_VERSION_MAJOR >= 1
			gst_registry_add_feature (gst_registry_get (), GST_PLUGIN_FEATURE (factory));
#else
			gst_registry_add_feature (gst_registry_get_default (), GST_PLUGIN_FEATURE (factory));
#endif


			return oldRank;
		}

		uint GetRank (const char *name)
		{
			const auto factory = gst_element_factory_find (name);
			if (!factory)
			{
				qWarning () << Q_FUNC_INFO
						<< "cannot find factory"
						<< name;
				return 0;
			}

			return gst_plugin_feature_get_rank (GST_PLUGIN_FEATURE (factory));
		}
	}

	void SourceObject::SetCurrentSource (const AudioSource& source)
	{
		IsSeeking_ = false;

		CurrentSource_ = source;

		Metadata_.clear ();

		if (source.ToUrl ().scheme ().startsWith ("http"))
			PrevSoupRank_ = SetSoupRank (G_MAXINT / 2);

		auto path = source.ToUrl ().toEncoded ();
		g_object_set (G_OBJECT (Dec_), "uri", path.constData (), nullptr);
	}

	void SourceObject::PrepareNextSource (const AudioSource& source)
	{
		NextSrcMutex_.lock ();

		qDebug () << Q_FUNC_INFO << source.ToUrl ();
		NextSource_ = source;

		NextSrcWC_.wakeAll ();
		NextSrcMutex_.unlock ();

		Metadata_.clear ();
	}

	void SourceObject::Play ()
	{
		if (CurrentSource_.IsEmpty ())
		{
			qDebug () << Q_FUNC_INFO
					<< "current source is invalid, setting next one";
			if (NextSource_.IsEmpty ())
				return;

			SetCurrentSource (NextSource_);
			NextSource_.Clear ();
		}

		gst_element_set_state (Path_->GetPipeline (), GST_STATE_PLAYING);
	}

	void SourceObject::Pause ()
	{
		if (!IsSeekable ())
			Stop ();
		else
			gst_element_set_state (Path_->GetPipeline (), GST_STATE_PAUSED);
	}

	void SourceObject::Stop ()
	{
		gst_element_set_state (Path_->GetPipeline (), GST_STATE_READY);
		Seek (0);
	}

	void SourceObject::Clear ()
	{
		ClearQueue ();
		CurrentSource_.Clear ();
		gst_element_set_state (Path_->GetPipeline (), GST_STATE_READY);
	}

	void SourceObject::ClearQueue ()
	{
		NextSource_.Clear ();
	}

	void SourceObject::HandleAboutToFinish ()
	{
		qDebug () << Q_FUNC_INFO;
		NextSrcMutex_.lock ();
		if (NextSource_.IsEmpty ())
		{
			emit aboutToFinish ();
			NextSrcWC_.wait (&NextSrcMutex_, 500);
		}
		qDebug () << "wait finished; next source:" << NextSource_.ToUrl ()
				<< "; current source:" << CurrentSource_.ToUrl ();

		std::shared_ptr<void> mutexGuard (nullptr,
				[this] (void*) { NextSrcMutex_.unlock (); });

		if (NextSource_.IsEmpty ())
		{
			qDebug () << Q_FUNC_INFO
					<< "no next source set, will stop playing";
			return;
		}

		SetCurrentSource (NextSource_);
		NextSource_.Clear ();
	}

	void SourceObject::HandleErrorMsg (GstMessage *msg)
	{
		GError *gerror = nullptr;
		gchar *debug = nullptr;
		gst_message_parse_error (msg, &gerror, &debug);

		const auto& msgStr = QString::fromUtf8 (gerror->message);
		const auto& debugStr = QString::fromUtf8 (debug);

		const auto code = gerror->code;

		g_error_free (gerror);
		g_free (debug);

		qWarning () << Q_FUNC_INFO
				<< code
				<< msgStr
				<< debugStr;

		SourceError errCode = SourceError::Other;
		switch (code)
		{
		case GST_CORE_ERROR_MISSING_PLUGIN:
			errCode = SourceError::MissingPlugin;
			break;
		default:
			break;
		}

		emit error (msgStr, errCode);
	}

	namespace
	{
		void FixEncoding (QString& out, const gchar *origStr)
		{
#ifdef WITH_LIBGUESS
			const auto& cp1252 = QTextCodec::codecForName ("CP-1252")->fromUnicode (origStr);
			if (cp1252.isEmpty ())
				return;

			const auto region = XmlSettingsManager::Instance ()
					.property ("TagsRecodingRegion").toString ();
			const auto encoding = libguess_determine_encoding (cp1252.constData (),
					cp1252.size (), region.toUtf8 ().constData ());
			if (!encoding)
				return;

			auto codec = QTextCodec::codecForName (encoding);
			if (!codec)
			{
				qWarning () << Q_FUNC_INFO
						<< "no codec for encoding"
						<< encoding;
				return;
			}

			const auto& proper = codec->toUnicode (cp1252.constData ());
			if (proper.isEmpty ())
				return;

			int origQCount = 0;
			while (*origStr)
				if (*(origStr++) == '?')
					++origQCount;

			if (origQCount >= proper.count ('?'))
				out = proper;
#else
			Q_UNUSED (out);
			Q_UNUSED (origStr);
#endif
		}

		void TagFunction (const GstTagList *list, const gchar *tag, gpointer data)
		{
			auto& map = *static_cast<SourceObject::TagMap_t*> (data);

			const auto& tagName = QString::fromUtf8 (tag).toLower ();
			auto& valList = map [tagName];

			switch (gst_tag_get_type (tag))
			{
			case G_TYPE_STRING:
			{
				gchar *str = nullptr;
				gst_tag_list_get_string (list, tag, &str);
				valList = QString::fromUtf8 (str);

				const auto recodingEnabled = XmlSettingsManager::Instance ()
						.property ("EnableTagsRecoding").toBool ();
				if (recodingEnabled &&
						(tagName == "title" || tagName == "album" || tagName == "artist"))
					FixEncoding (valList, str);

				g_free (str);
				break;
			}
			case G_TYPE_BOOLEAN:
			{
				int val = 0;
				gst_tag_list_get_boolean (list, tag, &val);
				valList = QString::number (val);
				break;
			}
			case G_TYPE_INT:
			{
				int val = 0;
				gst_tag_list_get_int (list, tag, &val);
				valList = QString::number (val);
				break;
			}
			case G_TYPE_UINT:
			{
				uint val = 0;
				gst_tag_list_get_uint (list, tag, &val);
				valList = QString::number (val);
				break;
			}
			case G_TYPE_FLOAT:
			{
				float val = 0;
				gst_tag_list_get_float (list, tag, &val);
				valList = QString::number (val);
				break;
			}
			case G_TYPE_DOUBLE:
			{
				double val = 0;
				gst_tag_list_get_double (list, tag, &val);
				valList = QString::number (val);
				break;
			}
			default:
				qWarning () << Q_FUNC_INFO
						<< "unhandled tag type"
						<< gst_tag_get_type (tag)
						<< "for"
						<< tag;
				break;
			}
		}
	}

	void SourceObject::HandleTagMsg (GstMessage *msg)
	{
		GstTagList *tagList = nullptr;
		gst_message_parse_tag (msg, &tagList);
		if (!tagList)
			return;

		const auto oldMetadata = Metadata_;
		gst_tag_list_foreach (tagList,
				TagFunction,
				&Metadata_);

		auto merge = [this] (const QString& oldName, const QString& stdName, bool emptyOnly)
		{
			if (Metadata_.contains (oldName) &&
					(!emptyOnly || Metadata_.value (stdName).isEmpty ()))
				Metadata_ [stdName] = Metadata_.value (oldName);
		};

		const auto& title = Metadata_.value ("title");
		const auto& split = title.split (" - ", QString::SkipEmptyParts);
		if (split.size () == 2 &&
				(!Metadata_.contains ("artist") ||
					Metadata_.value ("title") != split.value (1)))
		{
			Metadata_ ["artist"] = split.value (0);
			Metadata_ ["title"] = split.value (1);
		}

		merge ("organization", "album", true);
		merge ("genre", "title", true);

		if (oldMetadata != Metadata_)
			emit metaDataChanged ();
	}

	void SourceObject::HandleBufferingMsg (GstMessage *msg)
	{
		gint percentage = 0;
		gst_message_parse_buffering (msg, &percentage);

		emit bufferStatus (percentage);
	}

	namespace
	{
		SourceState GstToState (GstState state)
		{
			switch (state)
			{
			case GST_STATE_PAUSED:
				return SourceState::Paused;
			case GST_STATE_READY:
				return SourceState::Stopped;
			case GST_STATE_PLAYING:
				return SourceState::Playing;
			default:
				return SourceState::Error;
			}
		}
	}

	void SourceObject::HandleStateChangeMsg (GstMessage *msg)
	{
		if (msg->src != GST_OBJECT (Path_->GetPipeline ()))
			return;

		GstState oldState, newState, pending;
		gst_message_parse_state_changed (msg, &oldState, &newState, &pending);

		if (oldState == newState)
			return;

		if (IsSeeking_)
		{
			if (oldState == GST_STATE_PLAYING && newState == GST_STATE_PAUSED)
				return;

			if (oldState == GST_STATE_PAUSED && newState == GST_STATE_PLAYING)
			{
				IsSeeking_ = false;
				return;
			}
		}

		auto newNativeState = GstToState (newState);
		OldState_ = newNativeState;
		emit stateChanged (newNativeState, OldState_);
	}

	void SourceObject::HandleElementMsg (GstMessage *msg)
	{
		const auto msgStruct = gst_message_get_structure (msg);

#if GST_VERSION_MAJOR < 1
		if (gst_structure_has_name (msgStruct, "playbin2-stream-changed"))
#else
		if (gst_structure_has_name (msgStruct, "playbin-stream-changed"))
#endif
		{
			gchar *uri = nullptr;
			g_object_get (Dec_, "uri", &uri, nullptr);
			qDebug () << Q_FUNC_INFO << uri;
			g_free (uri);

			emit currentSourceChanged (CurrentSource_);
			emit metaDataChanged ();
		}
	}

	void SourceObject::HandleEosMsg (GstMessage*)
	{
		gst_element_set_state (Path_->GetPipeline (), GST_STATE_READY);
	}

	void SourceObject::SetupSource ()
	{
		GstElement *src;
		g_object_get (Dec_, "source", &src, nullptr);

		if (!CurrentSource_.ToUrl ().scheme ().startsWith ("http"))
			return;

		std::shared_ptr<void> soupRankGuard (nullptr,
				[&] (void*) -> void
				{
					if (PrevSoupRank_)
					{
						SetSoupRank (PrevSoupRank_);
						PrevSoupRank_ = 0;
					}
				});

		if (!g_object_class_find_property (G_OBJECT_GET_CLASS (src), "user-agent"))
		{
			qDebug () << Q_FUNC_INFO
					<< "user-agent property not found for"
					<< CurrentSource_.ToUrl ()
					<< (QString ("|") + G_OBJECT_TYPE_NAME (src) + "|")
					<< "soup rank:"
					<< GetRank ("souphttpsrc")
					<< "webkit rank:"
					<< GetRank ("WebKitWebSrc");
			return;
		}

		const auto& str = QString ("LeechCraft LMP/%1 (%2)")
				.arg (Core::Instance ().GetProxy ()->GetVersion ())
				.arg (gst_version_string ());
		qDebug () << Q_FUNC_INFO
				<< "setting user-agent to"
				<< str;
		g_object_set (src, "user-agent", str.toUtf8 ().constData (), nullptr);
	}

	void SourceObject::AddToPath (Path *path)
	{
		path->SetPipeline (Dec_);
		Path_ = path;
	}

	void SourceObject::PostAdd (Path *path)
	{
		auto bin = path->GetAudioBin ();
		g_object_set (GST_OBJECT (Dec_), "audio-sink", bin, nullptr);
	}

	void SourceObject::updateTotalTime ()
	{
		emit totalTimeChanged (GetTotalTime ());
	}

	void SourceObject::handleTick ()
	{
		emit tick (GetCurrentTime ());
	}
}
}

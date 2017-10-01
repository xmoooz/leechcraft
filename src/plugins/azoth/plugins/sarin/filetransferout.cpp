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

#include "filetransferout.h"
#include <tox/tox.h>
#include <util/threads/futures.h>
#include <util/sll/delayedexecutor.h>
#include <util/sll/either.h>
#include <util/sll/visitor.h>
#include "toxthread.h"
#include "util.h"
#include "threadexceptions.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Sarin
{
	FileTransferOut::FileTransferOut (const QString& azothId,
			const QByteArray& pubkey,
			const QString& filename,
			const std::shared_ptr<ToxThread>& thread,
			QObject *parent)
	: FileTransferBase { azothId, pubkey, thread, parent }
	, FilePath_ { filename }
	, File_ { filename }
	, Filesize_ { File_.size () }
	{
		if (!File_.open (QIODevice::ReadOnly))
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to open local file"
					<< filename;

			new Util::DelayedExecutor
			{
				[this]
				{
					emit errorAppeared (TEFileAccessError,
							tr ("Error opening local file: %1.")
								.arg (File_.errorString ()));
					emit stateChanged (TransferState::TSFinished);
				}
			};

			return;
		}

		using SendFileResult_t = Util::Either<TOX_ERR_FILE_SEND, uint32_t>;
		auto future = Thread_->ScheduleFunction ([this] (Tox *tox)
				{
					const auto& name = FilePath_.section ('/', -1, -1).toUtf8 ();

					const auto friendNum = GetFriendId (tox, PubKey_);;
					if (!friendNum)
						throw UnknownFriendException {};
					FriendNum_ = *friendNum;

					TOX_ERR_FILE_SEND error {};
					const auto result = tox_file_send (tox,
							FriendNum_,
							TOX_FILE_KIND_DATA,
							static_cast<uint64_t> (Filesize_),
							nullptr,
							reinterpret_cast<const uint8_t*> (name.constData ()),
							name.size (),
							&error);
					return result == UINT32_MAX ?
							SendFileResult_t::Left (error) :
							SendFileResult_t::Right (result);
				});
		Util::Sequence (this, future) >>
				[this] (const SendFileResult_t& result)
				{
					Util::Visit (result.AsVariant (),
							[this] (uint32_t filenum)
							{
								FileNum_ = filenum;
								emit stateChanged (TransferState::TSOffer);
							},
							[this] (TOX_ERR_FILE_SEND err)
							{
								qWarning () << Q_FUNC_INFO
										<< err;
								emit errorAppeared (TransferError::TEProtocolError,
										tr ("Tox file send error: %1")
												.arg (err));
								emit stateChanged (TransferState::TSFinished);
							});
				};
	}

	QString FileTransferOut::GetName () const
	{
		return FilePath_;
	}

	qint64 FileTransferOut::GetSize () const
	{
		return Filesize_;
	}

	TransferDirection FileTransferOut::GetDirection () const
	{
		return TransferDirection::TDOut;
	}

	void FileTransferOut::Accept (const QString&)
	{
	}

	void FileTransferOut::Abort ()
	{
	}

	void FileTransferOut::HandleAccept ()
	{
		State_ = State::Transferring;
		emit stateChanged (TSTransfer);
	}

	void FileTransferOut::HandleKill ()
	{
		TransferAllowed_ = false;
		emit errorAppeared (TEAborted, tr ("Remote party aborted file transfer."));
		emit stateChanged (TSFinished);
		State_ = State::Idle;
	}

	void FileTransferOut::HandlePause ()
	{
		TransferAllowed_ = false;
		State_ = State::Paused;
	}

	void FileTransferOut::HandleResume ()
	{
		TransferAllowed_ = true;
		State_ = State::Transferring;
	}

	void FileTransferOut::HandleFileControl (uint32_t friendNum, uint32_t fileNum, int type)
	{
		if (friendNum != FriendNum_ || fileNum != FileNum_)
			return;

		switch (State_)
		{
		case State::Waiting:
			switch (type)
			{
			case TOX_FILE_CONTROL_RESUME:
				HandleAccept ();
				break;
			case TOX_FILE_CONTROL_CANCEL:
				HandleKill ();
				break;
			default:
				qWarning () << Q_FUNC_INFO
						<< "unexpected control type in Waiting state:"
						<< type;
				break;
			}
			break;
		case State::Transferring:
			switch (type)
			{
			case TOX_FILE_CONTROL_CANCEL:
				HandleKill ();
				break;
			case TOX_FILE_CONTROL_PAUSE:
				HandlePause ();
				break;
			default:
				qWarning () << Q_FUNC_INFO
						<< "unexpected control type in Transferring state:"
						<< type;
				break;
			}
			break;
		case State::Paused:
			switch (type)
			{
			case TOX_FILE_CONTROL_RESUME:
				HandleResume ();
				break;
			case TOX_FILE_CONTROL_CANCEL:
				HandleKill ();
				break;
			default:
				qWarning () << Q_FUNC_INFO
						<< "unexpected control type in Killed state:"
						<< type;
				break;
			}
			break;
		}
	}

	void FileTransferOut::HandleChunkRequested (uint32_t friendNum, uint32_t fileNum, uint64_t offset, size_t length)
	{
		if (friendNum != FriendNum_ || fileNum != FileNum_)
			return;

		if (static_cast<uint64_t> (File_.pos ()) != offset)
			if (!File_.seek (offset))
			{
				qWarning () << Q_FUNC_INFO
						<< "cannot seek to"
						<< offset
						<< "while at pos"
						<< File_.pos ();
				return;
			}

		const auto& data = File_.read (length);
		if (static_cast<size_t> (data.size ()) != length)
			qWarning () << Q_FUNC_INFO
					<< "could not read"
					<< length
					<< "bytes, reading"
					<< data.size ()
					<< "instead";

		Thread_->ScheduleFunction ([data, friendNum, fileNum, offset] (Tox *tox)
				{
					TOX_ERR_FILE_SEND_CHUNK error {};
					const auto dataPtr = reinterpret_cast<const uint8_t*> (data.constData ());
					if (!tox_file_send_chunk (tox, friendNum, fileNum,
								offset, dataPtr, data.size (), &error))
						qWarning () << Q_FUNC_INFO
								<< "unable to send file chunk:"
								<< error;
				});
	}
}
}
}

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

#include <utility>

namespace LeechCraft
{
namespace Util
{
namespace IntSeq
{
	template<typename T, T... Fst, T... Snd>
	std::integer_sequence<T, Fst..., Snd...>
	ConcatImpl (std::integer_sequence<T, Fst...>, std::integer_sequence<T, Snd...>);

	template<typename... Seqs>
	struct ConcatS;

	template<typename... Seqs>
	using Concat = typename ConcatS<Seqs...>::Type_t;

	template<typename Seq>
	struct ConcatS<Seq>
	{
		using Type_t = Seq;
	};

	template<typename Seq1, typename Seq2, typename... Rest>
	struct ConcatS<Seq1, Seq2, Rest...>
	{
		using Type_t = Concat<decltype (ConcatImpl (Seq1 {}, Seq2 {})), Rest...>;
	};

	template<typename T, T E, size_t C>
	struct RepeatS
	{
		template<T... Is>
		static auto RepeatImpl (std::integer_sequence<T, Is...>)
		{
			return std::integer_sequence<T, (static_cast<void> (Is), E)...> {};
		}

		using Type_t = decltype (RepeatImpl (std::make_integer_sequence<T, C> {}));
	};

	template<typename T, T E, size_t C>
	using Repeat = typename RepeatS<T, E, C>::Type_t;
}
}
}

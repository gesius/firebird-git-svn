/*
 *	PROGRAM:	JRD International support
 *	MODULE:		CsConvert.h
 *	DESCRIPTION:	International text handling definitions
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Nickolay Samofatov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Nickolay Samofatov <nickolay@broadviewsoftware.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *  2006.10.10 Adriano dos Santos Fernandes - refactored from intl_classes.h
 *
 */

#ifndef JRD_CSCONVERT_H
#define JRD_CSCONVERT_H

#include "iberror.h"
#include "../common/classes/array.h"


namespace Jrd {

class CsConvert
{
public:
	CsConvert(charset* cs1, charset* cs2)
		: charSet1(cs1),
		  charSet2(cs2),
		  cnvt1((cs1 ? &cs1->charset_to_unicode : NULL)),
		  cnvt2((cs2 ? &cs2->charset_from_unicode : NULL))
	{
		if (cs1 == NULL)
		{
			cs1 = cs2;
			cnvt1 = cnvt2;

			cs2 = NULL;
			cnvt2 = NULL;
		}
	}

	CsConvert(const CsConvert& obj)
		: charSet1(obj.charSet1),
		  charSet2(obj.charSet2),
		  cnvt1(obj.cnvt1),
		  cnvt2(obj.cnvt2)
	{
	}

public:
	// CVC: Beware of this can of worms: csconvert_convert gets assigned
	// different functions that not necessarily take the same argument. Typically,
	// the src pointer and the dest pointer use different types.
	// How does this work without crashing is a miracle of IT.

	// To be used with getConvFromUnicode method of CharSet class
	ULONG convert(ULONG srcLen,
				  const USHORT* src,
				  ULONG dstLen,
				  UCHAR* dst,
				  ULONG* badInputPos = NULL,
				  bool ignoreTrailingSpaces = false)
	{
		return convert(srcLen, reinterpret_cast<const UCHAR*>(src), dstLen, dst, badInputPos, ignoreTrailingSpaces);
	}

	// To be used with getConvToUnicode method of CharSet class
	ULONG convert(ULONG srcLen,
				  const UCHAR* src,
				  ULONG dstLen,
				  USHORT* dst,
				  ULONG* badInputPos = NULL,
				  bool ignoreTrailingSpaces = false)
	{
		return convert(srcLen, src, dstLen, reinterpret_cast<UCHAR*>(dst), badInputPos, ignoreTrailingSpaces);
	}

	// To be used for arbitrary conversions
	ULONG convert(ULONG srcLen,
				  const UCHAR* src,
				  ULONG dstLen,
				  UCHAR* dst,
				  ULONG* badInputPos = NULL,
				  bool ignoreTrailingSpaces = false)
	{
		if (badInputPos)
			*badInputPos = srcLen;

		USHORT errCode = 0;
		ULONG errPos = 0;

		if (cnvt2)
		{
			Firebird::HalfStaticArray<UCHAR, BUFFER_SMALL> temp;

			ULONG len = (*cnvt1->csconvert_fn_convert)(cnvt1, srcLen, NULL, 0, NULL, &errCode, &errPos);

			if (len == INTL_BAD_STR_LENGTH || errCode != 0)
				Firebird::status_exception::raise(isc_arith_except, 0);

			temp.getBuffer(len);

			len = (*cnvt1->csconvert_fn_convert)(
				cnvt1, srcLen, src, len, temp.begin(), &errCode, &errPos);

			if (len == INTL_BAD_STR_LENGTH)
			{
				Firebird::status_exception::raise(
					isc_arith_except,
					isc_arg_gds, isc_transliteration_failed,
					0);
			}

			if (errCode == CS_BAD_INPUT && badInputPos)
				*badInputPos = errPos;
			else if (errCode != 0)
			{
				Firebird::status_exception::raise(
					isc_arith_except,
					isc_arg_gds, isc_transliteration_failed,
					0);
			}

			len = (*cnvt2->csconvert_fn_convert)(cnvt2, len, temp.begin(), dstLen, dst, &errCode, &errPos);

			if (len == INTL_BAD_STR_LENGTH)
			{
				Firebird::status_exception::raise(
					isc_arith_except,
					isc_arg_gds, isc_transliteration_failed,
					0);
			}
			else if (errCode != 0)
			{
				if (ignoreTrailingSpaces && errCode == CS_TRUNCATION_ERROR)
				{
					const USHORT* end = reinterpret_cast<const USHORT*>(temp.begin() + len);

					for (const USHORT* p = (const USHORT*) (temp.begin() + errPos);
						 p < end; ++p)
					{
						if (*p != 0x20)	// space
						{
							if (badInputPos)
							{
								*badInputPos = errPos;
								break;
							}
							else
								Firebird::status_exception::raise(isc_arith_except, 0);
						}
					}
				}
				else if (errCode == CS_TRUNCATION_ERROR)
				{
					if (badInputPos)
						*badInputPos = errPos;
					else
						Firebird::status_exception::raise(isc_arith_except, 0);
				}
				else
				{
					Firebird::status_exception::raise(
						isc_arith_except,
						isc_arg_gds, isc_transliteration_failed,
						0);
				}
			}

			return len;
		}
		else
		{
			ULONG len = (*cnvt1->csconvert_fn_convert)(
				cnvt1, srcLen, src, dstLen, dst, &errCode, &errPos);

			if (len == INTL_BAD_STR_LENGTH)
			{
				Firebird::status_exception::raise(
					isc_arith_except,
					isc_arg_gds, isc_transliteration_failed,
					0);
			}

			if (errCode == CS_BAD_INPUT && badInputPos)
				*badInputPos = errPos;
			else if (errCode != 0)
			{
				if (ignoreTrailingSpaces && errCode == CS_TRUNCATION_ERROR)
				{
					const UCHAR* end = src + srcLen - charSet1->charset_space_length;

					for (const UCHAR* p = src + errPos; p <= end; p += charSet1->charset_space_length)
					{
						if (memcmp(p, charSet1->charset_space_character,
								charSet1->charset_space_length) != 0)
						{
							if (badInputPos)
							{
								*badInputPos = errPos;
								break;
							}
							else
								Firebird::status_exception::raise(isc_arith_except, 0);
						}
					}
				}
				else if (errCode == CS_TRUNCATION_ERROR)
				{
					if (badInputPos)
						*badInputPos = errPos;
					else
						Firebird::status_exception::raise(isc_arith_except, 0);
				}
				else
				{
					Firebird::status_exception::raise(
						isc_arith_except,
						isc_arg_gds, isc_transliteration_failed,
						0);
				}
			}

			return len;
		}
	}

	// To be used for measure length of conversion
	ULONG convertLength(ULONG srcLen)
	{
		USHORT errCode;
		ULONG errPos;
		ULONG len = (*cnvt1->csconvert_fn_convert)(cnvt1, srcLen, NULL, 0, NULL, &errCode, &errPos);

		if (cnvt2)
		{
			if (len != INTL_BAD_STR_LENGTH && errCode == 0)
				len = (*cnvt2->csconvert_fn_convert)(cnvt2, len, NULL, 0, NULL, &errCode, &errPos);
		}

		if (len == INTL_BAD_STR_LENGTH || errCode != 0)
			Firebird::status_exception::raise(isc_arith_except, 0);

		return len;
	}

	const char* getName() const { return cnvt1->csconvert_name; }

	csconvert* getStruct() const { return cnvt1; }

private:
	charset* charSet1;
	charset* charSet2;
	csconvert* cnvt1;
	csconvert* cnvt2;
};

}	// namespace Jrd


#endif	// JRD_CSCONVERT_H

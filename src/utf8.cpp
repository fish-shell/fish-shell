/*
 * Copyright (c) 2007 Alexey Vatchenko <av@bsdua.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/types.h>
#include <stdint.h>  // IWYU pragma: keep
#include <string>
#include <limits>

#include "utf8.h"

#define _NXT	0x80
#define _SEQ2	0xc0
#define _SEQ3	0xe0
#define _SEQ4	0xf0
#define _SEQ5	0xf8
#define _SEQ6	0xfc

#define _BOM	0xfeff

/* We can tweak the following typedef to allow us to simulate Windows-style 16 bit wchar's on Unix */
typedef wchar_t utf8_wchar_t;
#define UTF8_WCHAR_MAX ((size_t)std::numeric_limits<utf8_wchar_t>::max())

typedef std::basic_string<utf8_wchar_t> utf8_wstring_t;

bool is_wchar_ucs2()
{
    return UTF8_WCHAR_MAX <= 0xFFFF;
}

static size_t utf8_to_wchar_internal(const char *in, size_t insize, utf8_wstring_t *result, int flags);
static size_t wchar_to_utf8_internal(const utf8_wchar_t *in, size_t insize, char *out, size_t outsize, int flags);

static bool safe_copy_wchar_to_utf8_wchar(const wchar_t *in, utf8_wchar_t *out, size_t count)
{
    bool result = true;
    for (size_t i=0; i < count; i++)
    {
        wchar_t c = in[i];
        if (c > UTF8_WCHAR_MAX)
        {
            result = false;
            break;
        }
        out[i] = c;
    }
    return result;
}

bool wchar_to_utf8_string(const std::wstring &str, std::string *result)
{
    result->clear();
    const size_t inlen = str.size();
    if (inlen == 0)
    {
        return true;
    }

    bool success = false;
    const wchar_t *input = str.c_str();
    size_t outlen = wchar_to_utf8(input, inlen, NULL, 0, 0);
    if (outlen > 0)
    {
        char *tmp = new char[outlen];
        size_t outlen2 = wchar_to_utf8(input, inlen, tmp, outlen, 0);
        if (outlen2 > 0)
        {
            result->assign(tmp, outlen2);
            success = true;
        }
        delete[] tmp;
    }
    return success;
}

size_t utf8_to_wchar(const char *in, size_t insize, std::wstring *out, int flags)
{
    if (in == NULL || insize == 0)
    {
        return 0;
    }

    size_t result;
    if (sizeof(wchar_t) == sizeof(utf8_wchar_t))
    {
        result = utf8_to_wchar_internal(in, insize, reinterpret_cast<utf8_wstring_t *>(out), flags);
    }
    else if (out == NULL)
    {
        result = utf8_to_wchar_internal(in, insize, NULL, flags);
    }
    else
    {
        // Allocate a temporary buffer to hold the output,
        // invoke the conversion with the temporary,
        // and then copy it back
        utf8_wstring_t tmp_output;
        result = utf8_to_wchar_internal(in, insize, &tmp_output, flags);
        out->insert(out->end(), tmp_output.begin(), tmp_output.end());
    }
    return result;
}

size_t wchar_to_utf8(const wchar_t *in, size_t insize, char *out, size_t outsize, int flags)
{
    if (in == NULL || insize == 0 || (outsize == 0 && out != NULL))
    {
        return 0;
    }

    size_t result;
    if (sizeof(wchar_t) == sizeof(utf8_wchar_t))
    {
        result = wchar_to_utf8_internal(reinterpret_cast<const utf8_wchar_t *>(in), insize, out, outsize, flags);
    }
    else
    {
        // Allocate a temporary buffer to hold the input
        // the std::copy performs the size conversion
        // note: insize may be 0
        utf8_wchar_t *tmp_input = new utf8_wchar_t[insize];
        if (! safe_copy_wchar_to_utf8_wchar(in, tmp_input, insize))
        {
            // our utf8_wchar_t is UCS-16 and there was an astral character
            result = 0;
        }
        else
        {
            // Invoke the conversion with the temporary, then clean up the input
            result = wchar_to_utf8_internal(tmp_input, insize, out, outsize, flags);
        }
        delete[] tmp_input;
    }
    return result;
}


static int __wchar_forbitten(utf8_wchar_t sym);
static int __utf8_forbitten(unsigned char octet);

static int
__wchar_forbitten(utf8_wchar_t sym)
{

    /* Surrogate pairs */
    if (sym >= 0xd800 && sym <= 0xdfff)
        return (-1);

    return (0);
}

static int
__utf8_forbitten(unsigned char octet)
{

    switch (octet)
    {
        case 0xc0:
        case 0xc1:
        case 0xf5:
        case 0xff:
            return (-1);
    }

    return (0);
}

/*
 * DESCRIPTION
 *	This function translates UTF-8 string into UCS-2 or UCS-4 string (all symbols
 *	will be in local machine byte order).
 *
 *	It takes the following arguments:
 *	in	- input UTF-8 string. It can be null-terminated.
 *	insize	- size of input string in bytes.
 *	out_string	- result buffer for UCS-2/4 string.
 *
 * RETURN VALUES
 *	The function returns size of result buffer (in wide characters).
 *	Zero is returned in case of error.
 *
 * CAVEATS
 *	1. If UTF-8 string contains zero symbols, they will be translated
 *	   as regular symbols.
 *	2. If UTF8_IGNORE_ERROR or UTF8_SKIP_BOM flag is set, sizes may vary
 *	   when `out' is NULL and not NULL. It's because of special UTF-8
 *	   sequences which may result in forbitten (by RFC3629) UNICODE
 *	   characters.  So, the caller must check return value every time and
 *	   not prepare buffer in advance (\0 terminate) but after calling this
 *	   function.
 */
static size_t utf8_to_wchar_internal(const char *in, size_t insize, utf8_wstring_t *out_string, int flags)
{
    unsigned char *p, *lim;
    utf8_wchar_t high;
    size_t n, total, i, n_bits;

    if (in == NULL || insize == 0)
        return (0);
    
    if (out_string != NULL)
        out_string->clear();

    total = 0;
    p = (unsigned char *)in;
    lim = p + insize;

    for (; p < lim; p += n)
    {
        if (__utf8_forbitten(*p) != 0 &&
                (flags & UTF8_IGNORE_ERROR) == 0)
            return (0);

        /*
         * Get number of bytes for one wide character.
         */
        n = 1;	/* default: 1 byte. Used when skipping bytes. */
        if ((*p & 0x80) == 0)
            high = (utf8_wchar_t)*p;
        else if ((*p & 0xe0) == _SEQ2)
        {
            n = 2;
            high = (utf8_wchar_t)(*p & 0x1f);
        }
        else if ((*p & 0xf0) == _SEQ3)
        {
            n = 3;
            high = (utf8_wchar_t)(*p & 0x0f);
        }
        else if ((*p & 0xf8) == _SEQ4)
        {
            n = 4;
            high = (utf8_wchar_t)(*p & 0x07);
        }
        else if ((*p & 0xfc) == _SEQ5)
        {
            n = 5;
            high = (utf8_wchar_t)(*p & 0x03);
        }
        else if ((*p & 0xfe) == _SEQ6)
        {
            n = 6;
            high = (utf8_wchar_t)(*p & 0x01);
        }
        else
        {
            if ((flags & UTF8_IGNORE_ERROR) == 0)
                return (0);
            continue;
        }

        /* does the sequence header tell us truth about length? */
        if (lim - p <= n - 1)
        {
            if ((flags & UTF8_IGNORE_ERROR) == 0)
                return (0);
            n = 1;
            continue;	/* skip */
        }

        /*
         * Validate sequence.
         * All symbols must have higher bits set to 10xxxxxx
         */
        if (n > 1)
        {
            for (i = 1; i < n; i++)
            {
                if ((p[i] & 0xc0) != _NXT)
                    break;
            }
            if (i != n)
            {
                if ((flags & UTF8_IGNORE_ERROR) == 0)
                    return (0);
                n = 1;
                continue;	/* skip */
            }
        }

        total++;
        if (out_string == NULL)
            continue;

        uint32_t out_val = 0;
        n_bits = 0;
        for (i = 1; i < n; i++)
        {
            out_val |= (utf8_wchar_t)(p[n - i] & 0x3f) << n_bits;
            n_bits += 6;		/* 6 low bits in every byte */
        }
        out_val |= high << n_bits;

        bool skip = false;
        if (__wchar_forbitten(out_val) != 0)
        {
            if ((flags & UTF8_IGNORE_ERROR) == 0)
            {
                return 0;	/* forbitten character */
            }
            else
            {
                skip = true;
            }
        }
        else if (out_val == _BOM && (flags & UTF8_SKIP_BOM) != 0)
        {
            skip = true;
        }

        if (skip)
        {
            total--;
        }
        else if (out_val > UTF8_WCHAR_MAX)
        {
            // wchar_t is UCS-2, but the UTF-8 specified an astral character
            return 0;
        }
        else
        {
            out_string->push_back(out_val);
        }
    }

    return (total);
}

/*
 * DESCRIPTION
 *	This function translates UCS-2/4 symbols (given in local machine
 *	byte order) into UTF-8 string.
 *
 *	It takes the following arguments:
 *	in	- input unicode string. It can be null-terminated.
 *	insize	- size of input string in wide characters.
 *	out	- result buffer for utf8 string. If out is NULL,
 *		function returns size of result buffer.
 *	outsize - size of result buffer.
 *
 * RETURN VALUES
 *	The function returns size of result buffer (in bytes). Zero is returned
 *	in case of error.
 *
 * CAVEATS
 *	If UCS-4 string contains zero symbols, they will be translated
 *	as regular symbols.
 */
static size_t wchar_to_utf8_internal(const utf8_wchar_t *in, size_t insize, char *out, size_t outsize, int flags)
{
    const utf8_wchar_t *w, *wlim;
    unsigned char *p, *lim;
    size_t total, n;

    if (in == NULL || insize == 0 || (outsize == 0 && out != NULL))
        return (0);

    w = in;
    wlim = w + insize;
    p = (unsigned char *)out;
    lim = p + outsize;
    total = 0;
    for (; w < wlim; w++)
    {
        if (__wchar_forbitten(*w) != 0)
        {
            if ((flags & UTF8_IGNORE_ERROR) == 0)
                return (0);
            else
                continue;
        }

        if (*w == _BOM && (flags & UTF8_SKIP_BOM) != 0)
            continue;

        const int32_t w_wide = *w;
        if (w_wide < 0)
        {
            if ((flags & UTF8_IGNORE_ERROR) == 0)
                return (0);
            continue;
        }
        else if (w_wide <= 0x0000007f)
            n = 1;
        else if (w_wide <= 0x000007ff)
            n = 2;
        else if (w_wide <= 0x0000ffff)
            n = 3;
        else if (w_wide <= 0x001fffff)
            n = 4;
        else if (w_wide <= 0x03ffffff)
            n = 5;
        else /* if (w_wide <= 0x7fffffff) */
            n = 6;

        total += n;

        if (out == NULL)
            continue;

        if (lim - p <= n - 1)
            return (0);		/* no space left */

        /* extract the wchar_t as big-endian. If wchar_t is UCS-16, the first two bytes will be 0 */
        unsigned char oc[4];
        uint32_t w_tmp = *w;
        oc[3] = w_tmp & 0xFF;
        w_tmp >>= 8;
        oc[2] = w_tmp & 0xFF;
        w_tmp >>= 8;
        oc[1] = w_tmp & 0xFF;
        w_tmp >>= 8;
        oc[0] = w_tmp & 0xFF;

        switch (n)
        {
            case 1:
                p[0] = oc[3];
                break;

            case 2:
                p[1] = _NXT | (oc[3] & 0x3f);
                p[0] = _SEQ2 | (oc[3] >> 6) | ((oc[2] & 0x07) << 2);
                break;

            case 3:
                p[2] = _NXT | (oc[3] & 0x3f);
                p[1] = _NXT | (oc[3] >> 6) | ((oc[2] & 0x0f) << 2);
                p[0] = _SEQ3 | ((oc[2] & 0xf0) >> 4);
                break;

            case 4:
                p[3] = _NXT | (oc[3] & 0x3f);
                p[2] = _NXT | (oc[3] >> 6) | ((oc[2] & 0x0f) << 2);
                p[1] = _NXT | ((oc[2] & 0xf0) >> 4) |
                       ((oc[1] & 0x03) << 4);
                p[0] = _SEQ4 | ((oc[1] & 0x1f) >> 2);
                break;

            case 5:
                p[4] = _NXT | (oc[3] & 0x3f);
                p[3] = _NXT | (oc[3] >> 6) | ((oc[2] & 0x0f) << 2);
                p[2] = _NXT | ((oc[2] & 0xf0) >> 4) |
                       ((oc[1] & 0x03) << 4);
                p[1] = _NXT | (oc[1] >> 2);
                p[0] = _SEQ5 | (oc[0] & 0x03);
                break;

            case 6:
                p[5] = _NXT | (oc[3] & 0x3f);
                p[4] = _NXT | (oc[3] >> 6) | ((oc[2] & 0x0f) << 2);
                p[3] = _NXT | (oc[2] >> 4) | ((oc[1] & 0x03) << 4);
                p[2] = _NXT | (oc[1] >> 2);
                p[1] = _NXT | (oc[0] & 0x3f);
                p[0] = _SEQ6 | ((oc[0] & 0x40) >> 6);
                break;
        }

        /*
         * NOTE: do not check here for forbitten UTF-8 characters.
         * They cannot appear here because we do proper convertion.
         */

        p += n;
    }

    return (total);
}

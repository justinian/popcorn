/* _PDCLIB_prepwrite( struct _PDCLIB_file_t * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>

int _PDCLIB_prepwrite( struct _PDCLIB_file_t * stream )
{
    if ( ( stream->bufidx < stream->bufend ) || ( stream->ungetidx > 0 ) ||
         ( stream->status & ( _PDCLIB_FREAD | _PDCLIB_ERRORFLAG | _PDCLIB_WIDESTREAM | _PDCLIB_EOFFLAG ) ) ||
         ! ( stream->status & ( _PDCLIB_FWRITE | _PDCLIB_FAPPEND | _PDCLIB_FRW ) ) )
    {
        /* Function called on illegal (e.g. input) stream.
           See the comments on implementation-defined errno values in
           <config.h>.
        */
        _PDCLIB_errno = _PDCLIB_ERROR;
        stream->status |= _PDCLIB_ERRORFLAG;
        return EOF;
    }
    stream->status |= _PDCLIB_FWRITE | _PDCLIB_BYTESTREAM;
    return 0;
}

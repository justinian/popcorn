/* fscanf( FILE *, const char *, ... )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <stdarg.h>

int fscanf( FILE * restrict stream, const char * restrict format, ... )
{
    int rc;
    va_list ap;
    va_start( ap, format );
    rc = vfscanf( stream, format, ap );
    va_end( ap );
    return rc;
}

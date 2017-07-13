/* tls.c (thread local storage) */

/***********************************************************************
*  This code is part of GLPK (GNU Linear Programming Kit).
*
*  Copyright (C) 2001-2013 Andrew Makhorin, Department for Applied
*  Informatics, Moscow Aviation Institute, Moscow, Russia. All rights
*  reserved. E-mail: <mao@gnu.org>.
*
*  GLPK is free software: you can redistribute it and/or modify it
*  under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  GLPK is distributed in the hope that it will be useful, but WITHOUT
*  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
*  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
*  License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with GLPK. If not, see <http://www.gnu.org/licenses/>.
***********************************************************************/

#include "glpenv.h"

#ifndef thread_local
# if __STDC_VERSION__ >= 201112 && !defined __STDC_NO_THREADS__
#  define thread_local _Thread_local
# elif defined _WIN32 && ( \
       defined _MSC_VER || \
       defined __ICL || \
       defined __DMC__ || \
       defined __BORLANDC__ )
#  define thread_local __declspec(thread)
/* note that ICC (linux) and Clang are covered by __GNUC__ */
# elif defined __GNUC__ || \
       defined __SUNPRO_C || \
       defined __xlC__
#  define thread_local __thread
# else
#  error "Cannot define thread_local"
# endif
#endif

static thread_local void *tls = NULL;

/***********************************************************************
*  NAME
*
*  tls_set_ptr - store global pointer in TLS
*
*  SYNOPSIS
*
*  #include "glpenv.h"
*  void tls_set_ptr(void *ptr);
*
*  DESCRIPTION
*
*  The routine tls_set_ptr stores a pointer specified by the parameter
*  ptr in the Thread Local Storage (TLS). */

void tls_set_ptr(void *ptr)
{     tls = ptr;
      return;
}

/***********************************************************************
*  NAME
*
*  tls_get_ptr - retrieve global pointer from TLS
*
*  SYNOPSIS
*
*  #include "glpenv.h"
*  void *tls_get_ptr(void);
*
*  RETURNS
*
*  The routine tls_get_ptr returns a pointer previously stored by the
*  routine tls_set_ptr. If the latter has not been called yet, NULL is
*  returned. */

void *tls_get_ptr(void)
{     void *ptr;
      ptr = tls;
      return ptr;
}

/* eof */

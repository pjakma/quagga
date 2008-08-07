/*
 * Routing Information Base header
 * Copyright (C) 1997 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#ifndef _QUAGGA_NEXTHOP_H
#define _QUAGGA_NEXTHOP_H

#include "prefix.h"

/* Nexthop structure. */
struct nexthop
{
  struct nexthop *next;
  struct nexthop *prev;

  u_char flags;
#define NEXTHOP_FLAG_ACTIVE     (1 << 0) /* This nexthop is alive. */
#define NEXTHOP_FLAG_FIB        (1 << 1) /* FIB nexthop. */
#define NEXTHOP_FLAG_RECURSIVE  (1 << 2) /* Recursive nexthop. */
#define NEXTHOP_FLAG_BLACKHOLE  (1 << 3) /* Stub nexthop for BH route */

  /* Nexthop address or interface name. */
  struct prefix *gate;

  /* Source address to use, if possible */
  struct prefix *src;
  
  /* Interface index. */
  unsigned int ifindex;
  
  /* Recursive lookup nexthop. */
  u_char rtype;
  unsigned int rifindex;
  struct prefix *rgate;
};

extern struct nexthop *nexthop_new (void);
extern void nexthop_free (struct nexthop *);

extern void nexthop_add (struct nexthop **, struct nexthop *);
extern void nexthop_delete (struct nexthop **, struct nexthop *);

extern int nexthop_same (struct nexthop *, struct nexthop *);

/* wrappers to maintain a user-supplied counter */
#define NEXTHOP_ADD(H,N,C) \
  do { \
    nexthop_add ((H),(N)); \
    (C)++; \
    assert ((C) > 0); \
  } while (0);

#define NEXTHOP_DEL(H,N,C) \
  do { \
    nexthop_delete ((H),(N)); \
    assert ((C) > 0); \
    (C)--; \
  } while (0);

#endif /*_QUAGGA_NEXTHOP_H */

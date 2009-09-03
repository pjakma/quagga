/* Nexthop structure.
 * Copyright (C) 1997, 98, 99, 2001 Kunihiro Ishiguro
 * Copyright (C) 2008 Paul Jakma
 *
 * This file is part of Quagga.
 *
 * Quagga is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Quagga is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Quagga; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <zebra.h>

#include "nexthop.h"
#include "memory.h"
#include "memtypes.h"
#include "if.h"

struct nexthop *
nexthop_new (void)
{
  struct nexthop *nh;
  nh = XCALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  nh->rifindex = nh->ifindex = IFINDEX_INTERNAL;
  return nh;
}

/* scrub contents of nexthop, but leave it allocated otherwise */
void
nexthop_scrub (struct nexthop *nh)
{
  if (nh->gate)
    prefix_free (nh->gate);
  if (nh->rgate)
    prefix_free (nh->gate);
  if (nh->src)
    prefix_free (nh->src);
  memset (nh, 0, sizeof (struct nexthop));
}
/* Free nexthop. */
void
nexthop_free (struct nexthop *nh)
{
  nexthop_scrub (nh);
  XFREE (MTYPE_NEXTHOP, nh);
}

/* deep copy nexthops */
void
nexthop_copy (struct nexthop *dst, struct nexthop *src)
{
  memcpy (dst, src, sizeof (struct nexthop));
  
  if (src->gate)
    prefix_copy ((dst->gate = prefix_new ()), src->gate);
  if (src->rgate)
    prefix_copy ((dst->rgate = prefix_new ()), src->rgate);
  if (src->src)
    prefix_copy ((dst->src = prefix_new ()), src->src);
}

int
nexthop_same (struct nexthop *next1, struct nexthop *next2)
{
  /* If set on either, then blackhole flag must be set on both */
  if (CHECK_FLAG (next1->flags, NEXTHOP_FLAG_BLACKHOLE)
      || CHECK_FLAG (next2->flags, NEXTHOP_FLAG_BLACKHOLE))
    {
      return (CHECK_FLAG (next1->flags, NEXTHOP_FLAG_BLACKHOLE)
              && CHECK_FLAG (next2->flags, NEXTHOP_FLAG_BLACKHOLE));
    }
  
  if (next1->ifindex != next2->ifindex)
    return 0;
  
/* If either is set: then they both must be, and they must be equal */
#define CMP_GATE(a,b) \
  if ((a) || (b)) \
    if (!(a) || !(b) || !prefix_same ((a), (b))) \
      return 0;
  
  CMP_GATE(next1->gate, next2->gate);
  
  if (CHECK_FLAG (next1->flags, NEXTHOP_FLAG_RECURSIVE)
      || CHECK_FLAG (next2->flags, NEXTHOP_FLAG_RECURSIVE))
    {
      if (!(CHECK_FLAG (next1->flags, NEXTHOP_FLAG_RECURSIVE)
            && CHECK_FLAG (next2->flags, NEXTHOP_FLAG_RECURSIVE)))
        return 0;
      
      if (next1->rifindex != next2->rifindex)
        return 0;
      
      CMP_GATE (next1->rgate, next2->rgate);
    }
  
  /* everything must be the same */
  return 1;
#undef CMP_GATE
}

void
nexthop_add (struct nexthop **head, struct nexthop *nexthop)
{
  struct nexthop *last;
  
  for (last = *head; last && last->next; last = last->next)
    ;
  if (last)
    last->next = nexthop;
  else
    *head = nexthop;
  nexthop->prev = last;
}

void
nexthop_delete (struct nexthop **head, struct nexthop *nexthop)
{
  if (nexthop->next)
    nexthop->next->prev = nexthop->prev;
  if (nexthop->prev)
    nexthop->prev->next = nexthop->next;
  else
    *head = nexthop->next;
}

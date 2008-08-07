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

#ifndef _ZEBRA_RIB_H
#define _ZEBRA_RIB_H

#include "prefix.h"
#include "nexthop.h"

#define DISTANCE_INFINITY  255

/* Routing information base. */

struct rib
{
  /* Status Flags for the *route_node*, but kept in the head RIB.. */
  u_char rn_status;
#define RIB_ROUTE_QUEUED(x)	(1 << (x))

  /* Link list. */
  struct rib *next;
  struct rib *prev;
  
  /* Nexthop structure */
  struct nexthop *nexthop;
  
  /* Refrence count. */
  unsigned long refcnt;
  
  /* Uptime. */
  time_t uptime;

  /* Type fo this route. */
  int type;

  /* Which routing table */
  int table;			

  /* Metric */
  u_int32_t metric;

  /* Distance. */
  u_char distance;

  /* Flags of this route.
   * This flag's definition is in lib/zebra.h ZEBRA_FLAG_* and is exposed
   * to clients via Zserv
   */
  u_char flags;

  /* RIB internal status */
  u_char status;
#define RIB_ENTRY_REMOVED	(1 << 0)

  /* Nexthop information. */
  u_char nexthop_num;
  u_char nexthop_active_num;
  u_char nexthop_fib_num;
};

/* meta-queue structure:
 * sub-queue 0: connected, kernel
 * sub-queue 1: static
 * sub-queue 2: RIP, RIPng, OSPF, OSPF6, IS-IS
 * sub-queue 3: iBGP, eBGP
 * sub-queue 4: any other origin (if any)
 */
#define MQ_SIZE 5
struct meta_queue
{
  struct list *subq[MQ_SIZE];
  u_int32_t size; /* sum of lengths of all subqueues */
};

/* Static route information. */
struct static_route
{
  /* For linked list. */
  struct static_route *prev;
  struct static_route *next;
  
  u_char flags;
  /* May set ZEBRA_FLAG_BLACKHOLE, may additionally set ZEBRA_FLAG_REJECT */

  /* Administrative distance. */
  u_char distance;

  struct prefix *gate;
  char *ifname; 
};

//enum nexthop_types_t
//{
//  NEXTHOP_TYPE_IFINDEX = 1,      /* Directly connected.  */
//  NEXTHOP_TYPE_IPV4,             /* IPv4 nexthop.  */
//  NEXTHOP_TYPE_IPV6,             /* IPv6 nexthop.  */
//  NEXTHOP_TYPE_BLACKHOLE,        /* Null0 nexthop.  */
//};

/* Routing table instance.  */
struct vrf
{
  /* Identifier.  This is same as routing table vector index.  */
  u_int32_t id;

  /* Routing table name.  */
  char *name;

  /* Description.  */
  char *desc;

  /* FIB identifier.  */
  u_char fib_id;

  /* Routing table.  */
  struct route_table *table[AFI_MAX][SAFI_MAX];

  /* Static route configuration.  */
  struct route_table *stable[AFI_MAX][SAFI_MAX];
};

extern void rib_nexthop_blackhole_add (struct rib *);
extern void rib_nexthop_add (struct rib *, struct prefix *,
			     struct prefix *, unsigned int ifindex);
extern void rib_lookup_and_dump (struct prefix *);
extern void rib_dump (const char *, const struct prefix *, const struct rib *);
extern int rib_lookup_route (struct prefix *, struct prefix *);
#define ZEBRA_RIB_LOOKUP_ERROR -1
#define ZEBRA_RIB_FOUND_EXACT 0
#define ZEBRA_RIB_FOUND_NOGATE 1
#define ZEBRA_RIB_FOUND_CONNECTED 2
#define ZEBRA_RIB_NOTFOUND 3

extern struct vrf *vrf_lookup (u_int32_t);
extern struct route_table *vrf_table (afi_t afi, safi_t safi, u_int32_t id);
extern struct route_table *vrf_static_table (afi_t afi, safi_t safi, u_int32_t id);

/* NOTE:
 * All rib_add functions will not just add prefix into RIB, but
 * also implicitly withdraw equal prefix of same type. */
extern int rib_add (int type, int flags, struct prefix *p, 
			 struct prefix *gate, struct prefix *src,
			 unsigned int ifindex, u_int32_t vrf_id,
			 u_int32_t, u_char);

extern int rib_add_multipath (struct prefix *, struct rib *);

extern int rib_delete (int type, int flags, struct prefix *p,
		            struct prefix *gate, unsigned int ifindex, 
		            u_int32_t);

extern struct rib *rib_match (struct prefix *);
extern struct rib *rib_lookup (struct prefix *);

extern void rib_update (void);
extern void rib_weed_tables (void);
extern void rib_sweep_route (void);
extern void rib_close (void);
extern void rib_init (void);

extern int static_add (struct prefix *p, struct prefix *gate,
                       const char *ifname, u_char flags, u_char distance,
                       u_int32_t vrf_id);

extern int static_delete (struct prefix *p, struct prefix *gate, 
                          const char *ifname, u_char distance,
                          u_int32_t vrf_id);

#endif /*_ZEBRA_RIB_H */

/* Zebra VTY functions
 * Copyright (C) 2002 Kunihiro Ishiguro
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA.  
 */

#include <zebra.h>

#include "memory.h"
#include "if.h"
#include "prefix.h"
#include "command.h"
#include "table.h"
#include "rib.h"

#include "zebra/zserv.h"

/* General fucntion for static route. */
static int
zebra_static (struct vty *vty, int add_cmd, const char *dest_str,
              const char *mask_str, const char *gate_str, const char *ifname,
              const char *flag_str, const char *distance_str)
{
  int ret;
  u_char distance = ZEBRA_STATIC_DISTANCE_DEFAULT;
  struct prefix p;
  struct prefix g;
  struct prefix *gate = &g;
  struct in_addr mask;
  
  u_char flag = 0;
  
  ret = str2prefix (dest_str, &p);
  if (ret <= 0)
    {
      vty_out (vty, "%% Malformed address%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Cisco like mask notation. */
  if (mask_str)
    {
      if (p.family != AF_INET)
        {
          vty_out (vty, "%% Address mask only valid with IPv4 prefixes%s",
                         VTY_NEWLINE);
          return CMD_WARNING;
        }
      
      ret = inet_aton (mask_str, &mask);
      if (ret == 0)
        {
          vty_out (vty, "%% Malformed address mask%s", VTY_NEWLINE);
          return CMD_WARNING;
        }
      p.prefixlen = ip_masklen (mask);
    }

  /* Apply mask for given prefix. */
  apply_mask (&p);

  /* Administrative distance. */
  if (distance_str)
    VTY_GET_INTEGER_RANGE("administrative distance", distance, distance_str,
                          1, DISTANCE_INFINITY);

  /* Null0 static route.  */
  if ((gate_str != NULL)
      && (strncasecmp (gate_str, "Null0", strlen (gate_str)) == 0))
    {
      if (flag_str)
        {
          vty_out (vty, "%% can not have flag %s with Null0%s",
                   flag_str, VTY_NEWLINE);
          return CMD_WARNING;
        }
      if (add_cmd)
        static_add (&p, NULL, NULL, ZEBRA_FLAG_BLACKHOLE, distance, 0);
      else
        static_delete (&p, NULL, NULL, distance, 0);
      return CMD_SUCCESS;
    }

  /* Route flags */
  if (flag_str) {
    switch(flag_str[0]) {
      case 'r':
      case 'R': /* XXX */
        SET_FLAG (flag, ZEBRA_FLAG_REJECT);
        break;
      case 'b':
      case 'B': /* XXX */
        SET_FLAG (flag, ZEBRA_FLAG_BLACKHOLE);
        break;
      default:
        vty_out (vty, "%% Malformed flag %s %s", flag_str, VTY_NEWLINE);
        return CMD_WARNING;
    }
  }
  
  if (gate_str == NULL)
    {
      if (add_cmd)
        {
          /* adding just a prefix requires flags */
          if (!flag)
            {
              /* this should already be caught by the vty layer, but just
               * in case, 
               */
              vty_out (vty, "%% Command incomplete%s", VTY_NEWLINE);
              return CMD_WARNING;
            }
          
          static_add (&p, NULL, NULL, flag, distance, 0);
        }
      else
        static_delete (&p, NULL, NULL, distance, 0);

      return CMD_SUCCESS;
    }
  
  /* When gateway is in IP format, gate is treated as nexthop
     address other case gate is treated as interface name. */
  ret = str2prefix (gate_str, gate);
  
  /* Filter out some invalid cases. Note that there's quite a matrix
   * of possibilities here..
   *
   * Also: IPv6 interface route requires a next-hop.
   */
   
  /* gateway looks like a prefix, so sanity check that. */
  if (ret > 0)
    {
      if (gate->prefixlen < PREFIX_MAX_PLEN(gate))
        {
          vty_out (vty, "%% Gateway requires a host address%s", VTY_NEWLINE);
          return CMD_WARNING;
        }
      
      /* Should be caught by the VTY and the command definition normally */
      if (gate->family != p.family)
        {
          vty_out (vty, "%% Prefix and nexthop address-family mismatch%s",
                   VTY_NEWLINE);
          return CMD_WARNING;
        }
    }
  
  /* Gateway str must be an interface, so update ifname */
  if (ret == 0)
    {
      if (ifname)
        {
          vty_out (vty, "%% One of the route destinations"
                        " must be an IP gateway%s", VTY_NEWLINE);
          return CMD_WARNING;
        }
      gate = NULL;
      ifname = gate_str;
    }
  
  
  if (add_cmd)
    static_add (&p, gate, ifname, flag, distance, 0);
  else
    static_delete (&p, gate, ifname, distance, 0);

  return CMD_SUCCESS;
}

/* Static route configuration.  */
DEFUN (ip_route, 
       ip_route_cmd,
       "ip route A.B.C.D/M (A.B.C.D|INTERFACE|null0) [<1-255>]",
       IP_STR
       "Establish static routes\n"
       "IP destination prefix (e.g. 10.0.0.0/8)\n"
       "IP gateway address\n"
       "IP gateway interface name\n"
       "Null interface\n"
       "Distance value for this prefix\n")
{
  const short distpos = 2;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 1, argv[0], NULL, argv[1], NULL, NULL, dist);
}

DEFUN (ip_route_flags,
       ip_route_flags_cmd,
       "ip route A.B.C.D/M (reject|blackhole) [<1-255>]",
       IP_STR
       "Establish static routes\n"
       "IP destination prefix (e.g. 10.0.0.0/8)\n"
       "Emit an ICMP unreachable when matched\n"
       "Silently discard pkts when matched\n"
       "Distance value for this prefix\n")
{
  const short distpos = 2;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 1, argv[0], NULL, NULL, NULL, argv[1], dist);
}


DEFUN (no_ip_route, 
       no_ip_route_cmd,
       "no ip route A.B.C.D/M (A.B.C.D|INTERFACE|null0) [<1-255>]",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IP destination prefix (e.g. 10.0.0.0/8)\n"
       "IP gateway address\n"
       "IP gateway interface name\n"
       "Null interface\n"
       "Distance value for this prefix\n")
{
  const short distpos = 2;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 0, argv[0], NULL, argv[1], NULL, NULL, dist);
}

DEFUN (no_ip_route_flags,
       no_ip_route_flags_cmd,
       "no ip route A.B.C.D/M (reject|blackhole) [<1-255>]",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IP destination prefix (e.g. 10.0.0.0/8)\n"
       "Emit an ICMP unreachable when matched\n"
       "Silently discard pkts when matched\n"
       "Distance value for this prefix\n")
{
  const short distpos = 3;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  return zebra_static (vty, 0, argv[0], NULL, NULL, NULL, argv[1], dist);
}

char *proto_rm[AFI_MAX][ZEBRA_ROUTE_MAX+1];	/* "any" == ZEBRA_ROUTE_MAX */

DEFUN (ip_protocol,
       ip_protocol_cmd,
       "ip protocol PROTO route-map ROUTE-MAP",
       NO_STR
       "Apply route map to PROTO\n"
       "Protocol name\n"
       "Route map name\n")
{
  int i;

  if (strcasecmp(argv[0], "any") == 0)
    i = ZEBRA_ROUTE_MAX;
  else
    i = proto_name2num(argv[0]);
  if (i < 0)
    {
      vty_out (vty, "invalid protocol name \"%s\"%s", argv[0] ? argv[0] : "",
               VTY_NEWLINE);
      return CMD_WARNING;
    }
  if (proto_rm[AFI_IP][i])
    XFREE (MTYPE_ROUTE_MAP_NAME, proto_rm[AFI_IP][i]);
  proto_rm[AFI_IP][i] = XSTRDUP (MTYPE_ROUTE_MAP_NAME, argv[1]);
  return CMD_SUCCESS;
}

DEFUN (no_ip_protocol,
       no_ip_protocol_cmd,
       "no ip protocol PROTO",
       NO_STR
       "Remove route map from PROTO\n"
       "Protocol name\n")
{
  int i;

  if (strcasecmp(argv[0], "any") == 0)
    i = ZEBRA_ROUTE_MAX;
  else
    i = proto_name2num(argv[0]);
  if (i < 0)
    {
      vty_out (vty, "invalid protocol name \"%s\"%s", argv[0] ? argv[0] : "",
               VTY_NEWLINE);
     return CMD_WARNING;
    }
  if (proto_rm[AFI_IP][i])
    XFREE (MTYPE_ROUTE_MAP_NAME, proto_rm[AFI_IP][i]);
  proto_rm[AFI_IP][i] = NULL;
  return CMD_SUCCESS;
}

/* Print the nexthop, in 'show ip route' style line */
static void
vty_show_nexthop_line (struct vty *vty, struct nexthop *nexthop)
{
  char buf[INET6_ADDRSTRLEN + 3];
  
  do {
    if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_BLACKHOLE))
      {
        vty_out (vty, " directly connected, Null0");
        break;
      }
    
    if (nexthop->gate)
      {
        vty_out (vty, " %s", inet_ntop (nexthop->gate->family,
                                        &nexthop->gate->u.prefix,
                                        buf, sizeof(buf)));
      }
    
    if (nexthop->ifindex && nexthop->gate)
        vty_out (vty, ", via %s", ifindex2ifname (nexthop->ifindex));

    if (nexthop->ifindex && !nexthop->gate)
      vty_out (vty, " directly connected, %s",
               ifindex2ifname (nexthop->ifindex));
  } while (0);
  
  if (! CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE))
    vty_out (vty, " inactive");

  if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE))
    {
      vty_out (vty, " (recursive");
        
        if (nexthop->rgate)
          {
            prefix2str (nexthop->rgate, buf, sizeof(buf));
            vty_out (vty, " via %s)", buf);
          }
        else if (nexthop->rifindex)
          vty_out (vty, " is directly connected, %s)",
                   ifindex2ifname (nexthop->rifindex));
    }
  if (nexthop->src)
    {
      prefix2str (nexthop->src, buf, sizeof(buf));
      vty_out (vty, ", src %s", buf);
    }
}

/* New RIB.  Detailed information for IPv4 route. */
static void
vty_show_ip_route_detail (struct vty *vty, struct route_node *rn)
{
  struct rib *rib;
  struct nexthop *nexthop;
  char buf[INET6_ADDRSTRLEN + 3];

  for (rib = rn->info; rib; rib = rib->next)
    {
      prefix2str (&rn->p, buf, sizeof(buf));
      vty_out (vty, "Routing entry for %s%s", 
	       buf, VTY_NEWLINE);
      vty_out (vty, "  Known via \"%s\"", zebra_route_string (rib->type));
      vty_out (vty, ", distance %d, metric %d", rib->distance, rib->metric);
      if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED))
	vty_out (vty, ", best");
      if (rib->refcnt)
	vty_out (vty, ", refcnt %ld", rib->refcnt);
      if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_BLACKHOLE))
       vty_out (vty, ", blackhole");
      if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_REJECT))
       vty_out (vty, ", reject");
      vty_out (vty, "%s", VTY_NEWLINE);

#define ONE_DAY_SECOND 60*60*24
#define ONE_WEEK_SECOND 60*60*24*7
      if (rib->type > ZEBRA_ROUTE_CONNECT)
	{
	  time_t uptime;
	  struct tm *tm;

	  uptime = time (NULL);
	  uptime -= rib->uptime;
	  tm = gmtime (&uptime);

	  vty_out (vty, "  Last update ");

	  if (uptime < ONE_DAY_SECOND)
	    vty_out (vty,  "%02d:%02d:%02d", 
		     tm->tm_hour, tm->tm_min, tm->tm_sec);
	  else if (uptime < ONE_WEEK_SECOND)
	    vty_out (vty, "%dd%02dh%02dm", 
		     tm->tm_yday, tm->tm_hour, tm->tm_min);
	  else
	    vty_out (vty, "%02dw%dd%02dh", 
		     tm->tm_yday/7,
		     tm->tm_yday - ((tm->tm_yday/7) * 7), tm->tm_hour);
	  vty_out (vty, " ago%s", VTY_NEWLINE);
	}

      for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
	{
	  vty_out (vty, "  %c",
		   CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB) ? '*' : ' ');
          
          vty_show_nexthop_line (vty, nexthop);
          
	  vty_out (vty, "%s", VTY_NEWLINE);
	}
      vty_out (vty, "%s", VTY_NEWLINE);
    }
}

static void
vty_show_ip_route (struct vty *vty, struct route_node *rn, struct rib *rib)
{
  struct nexthop *nexthop;
  int len = 0;
  char buf[BUFSIZ];

  /* Nexthop information. */
  for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
    {
      if (nexthop == rib->nexthop)
	{
	  /* Prefix information. */
	  len = vty_out (vty, "%c%c%c %s/%d",
			 zebra_route_char (rib->type),
			 CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED)
			 ? '>' : ' ',
			 CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB)
			 ? '*' : ' ',
			 inet_ntop (rn->p.family, &rn->p.u.prefix, buf, BUFSIZ),
			 rn->p.prefixlen);
		
	  /* Distance and metric display. */
	  if (rib->type != ZEBRA_ROUTE_CONNECT 
	      && rib->type != ZEBRA_ROUTE_KERNEL)
	    len += vty_out (vty, " [%d/%d]", rib->distance,
			    rib->metric);
	}
      else
	vty_out (vty, "  %c%*c",
		 CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB)
		 ? '*' : ' ',
		 len - 3, ' ');
      
      vty_show_nexthop_line (vty, nexthop);
      
      if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_BLACKHOLE))
               vty_out (vty, ", bh");
      if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_REJECT))
               vty_out (vty, ", rej");

      if (rib->type > ZEBRA_ROUTE_CONNECT)
	{
	  time_t uptime;
	  struct tm *tm;

	  uptime = time (NULL);
	  uptime -= rib->uptime;
	  tm = gmtime (&uptime);

#define ONE_DAY_SECOND 60*60*24
#define ONE_WEEK_SECOND 60*60*24*7

	  if (uptime < ONE_DAY_SECOND)
	    vty_out (vty,  ", %02d:%02d:%02d", 
		     tm->tm_hour, tm->tm_min, tm->tm_sec);
	  else if (uptime < ONE_WEEK_SECOND)
	    vty_out (vty, ", %dd%02dh%02dm", 
		     tm->tm_yday, tm->tm_hour, tm->tm_min);
	  else
	    vty_out (vty, ", %02dw%dd%02dh", 
		     tm->tm_yday/7,
		     tm->tm_yday - ((tm->tm_yday/7) * 7), tm->tm_hour);
	}
      vty_out (vty, "%s", VTY_NEWLINE);
    }
}

#define SHOW_ROUTE_V4_HEADER "Codes: K - kernel route, C - connected, " \
  "S - static, R - RIP, O - OSPF,%s       I - ISIS, B - BGP, " \
  "> - selected route, * - FIB route%s%s"

DEFUN (show_ip_route,
       show_ip_route_cmd,
       "show ip route",
       SHOW_STR
       IP_STR
       "IP routing table\n")
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  int first = 1;

  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  /* Show all IPv4 routes. */
  for (rn = route_top (table); rn; rn = route_next (rn))
    for (rib = rn->info; rib; rib = rib->next)
      {
	if (first)
	  {
	    vty_out (vty, SHOW_ROUTE_V4_HEADER, VTY_NEWLINE, VTY_NEWLINE,
		     VTY_NEWLINE);
	    first = 0;
	  }
	vty_show_ip_route (vty, rn, rib);
      }
  return CMD_SUCCESS;
}

DEFUN (show_ip_route_prefix_longer,
       show_ip_route_prefix_longer_cmd,
       "show ip route A.B.C.D/M longer-prefixes",
       SHOW_STR
       IP_STR
       "IP routing table\n"
       "IP prefix <network>/<length>, e.g., 35.0.0.0/8\n"
       "Show route matching the specified Network/Mask pair only\n")
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  struct prefix p;
  int ret;
  int first = 1;

  ret = str2prefix (argv[0], &p);
  if (! ret)
    {
      vty_out (vty, "%% Malformed Prefix%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  
  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  /* Show matched type IPv4 routes. */
  for (rn = route_top (table); rn; rn = route_next (rn))
    for (rib = rn->info; rib; rib = rib->next)
      if (prefix_match (&p, &rn->p))
	{
	  if (first)
	    {
	      vty_out (vty, SHOW_ROUTE_V4_HEADER, VTY_NEWLINE,
		       VTY_NEWLINE, VTY_NEWLINE);
	      first = 0;
	    }
	  vty_show_ip_route (vty, rn, rib);
	}
  return CMD_SUCCESS;
}

DEFUN (show_ip_route_supernets,
       show_ip_route_supernets_cmd,
       "show ip route supernets-only",
       SHOW_STR
       IP_STR
       "IP routing table\n"
       "Show supernet entries only\n")
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  u_int32_t addr; 
  int first = 1;

  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  /* Show matched type IPv4 routes. */
  for (rn = route_top (table); rn; rn = route_next (rn))
    for (rib = rn->info; rib; rib = rib->next)
      {
	addr = ntohl (rn->p.u.prefix4.s_addr);

	if ((IN_CLASSC (addr) && rn->p.prefixlen < 24)
	   || (IN_CLASSB (addr) && rn->p.prefixlen < 16)
	   || (IN_CLASSA (addr) && rn->p.prefixlen < 8)) 
	  {
	    if (first)
	      {
		vty_out (vty, SHOW_ROUTE_V4_HEADER, VTY_NEWLINE,
			 VTY_NEWLINE, VTY_NEWLINE);
		first = 0;
	      }
	    vty_show_ip_route (vty, rn, rib);
	  }
      }
  return CMD_SUCCESS;
}

DEFUN (show_ip_route_protocol,
       show_ip_route_protocol_cmd,
       "show ip route (bgp|connected|isis|kernel|ospf|rip|static)",
       SHOW_STR
       IP_STR
       "IP routing table\n"
       "Border Gateway Protocol (BGP)\n"
       "Connected\n"
       "ISO IS-IS (ISIS)\n"
       "Kernel\n"
       "Open Shortest Path First (OSPF)\n"
       "Routing Information Protocol (RIP)\n"
       "Static routes\n")
{
  zebra_route_t type;
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  int first = 1;

  if (strncmp (argv[0], "b", 1) == 0)
    type = ZEBRA_ROUTE_BGP;
  else if (strncmp (argv[0], "c", 1) == 0)
    type = ZEBRA_ROUTE_CONNECT;
  else if (strncmp (argv[0], "k", 1) ==0)
    type = ZEBRA_ROUTE_KERNEL;
  else if (strncmp (argv[0], "o", 1) == 0)
    type = ZEBRA_ROUTE_OSPF;
  else if (strncmp (argv[0], "i", 1) == 0)
    type = ZEBRA_ROUTE_ISIS;
  else if (strncmp (argv[0], "r", 1) == 0)
    type = ZEBRA_ROUTE_RIP;
  else if (strncmp (argv[0], "s", 1) == 0)
    type = ZEBRA_ROUTE_STATIC;
  else 
    {
      vty_out (vty, "Unknown route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  
  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  /* Show matched type IPv4 routes. */
  for (rn = route_top (table); rn; rn = route_next (rn))
    for (rib = rn->info; rib; rib = rib->next)
      if (rib->type == type)
	{
	  if (first)
	    {
	      vty_out (vty, SHOW_ROUTE_V4_HEADER,
		       VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE);
	      first = 0;
	    }
	  vty_show_ip_route (vty, rn, rib);
	}
  return CMD_SUCCESS;
}

DEFUN (show_ip_route_addr,
       show_ip_route_addr_cmd,
       "show ip route A.B.C.D",
       SHOW_STR
       IP_STR
       "IP routing table\n"
       "Network in the IP routing table to display\n")
{
  int ret;
  struct prefix_ipv4 p;
  struct route_table *table;
  struct route_node *rn;

  ret = str2prefix_ipv4 (argv[0], &p);
  if (ret <= 0)
    {
      vty_out (vty, "%% Malformed IPv4 address%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  rn = route_node_match (table, (struct prefix *) &p);
  if (! rn)
    {
      vty_out (vty, "%% Network not in table%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  vty_show_ip_route_detail (vty, rn);

  route_unlock_node (rn);

  return CMD_SUCCESS;
}

DEFUN (show_ip_route_prefix,
       show_ip_route_prefix_cmd,
       "show ip route A.B.C.D/M",
       SHOW_STR
       IP_STR
       "IP routing table\n"
       "IP prefix <network>/<length>, e.g., 35.0.0.0/8\n")
{
  int ret;
  struct prefix_ipv4 p;
  struct route_table *table;
  struct route_node *rn;

  ret = str2prefix_ipv4 (argv[0], &p);
  if (ret <= 0)
    {
      vty_out (vty, "%% Malformed IPv4 address%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  rn = route_node_match (table, (struct prefix *) &p);
  if (! rn || rn->p.prefixlen != p.prefixlen)
    {
      vty_out (vty, "%% Network not in table%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  vty_show_ip_route_detail (vty, rn);

  route_unlock_node (rn);

  return CMD_SUCCESS;
}

static void
vty_show_ip_route_summary (struct vty *vty, struct route_table *table)
{
  struct route_node *rn;
  struct rib *rib;
  struct nexthop *nexthop;
#define ZEBRA_ROUTE_IBGP  ZEBRA_ROUTE_MAX
#define ZEBRA_ROUTE_TOTAL (ZEBRA_ROUTE_IBGP + 1)
  u_int32_t rib_cnt[ZEBRA_ROUTE_TOTAL + 1];
  u_int32_t fib_cnt[ZEBRA_ROUTE_TOTAL + 1];
  u_int32_t i;

  memset (&rib_cnt, 0, sizeof(rib_cnt));
  memset (&fib_cnt, 0, sizeof(fib_cnt));
  for (rn = route_top (table); rn; rn = route_next (rn))
    for (rib = rn->info; rib; rib = rib->next)
      for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
        {
	  rib_cnt[ZEBRA_ROUTE_TOTAL]++;
	  rib_cnt[rib->type]++;
	  if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB)) 
	    {
	      fib_cnt[ZEBRA_ROUTE_TOTAL]++;
	      fib_cnt[rib->type]++;
	    }
	  if (rib->type == ZEBRA_ROUTE_BGP && 
	      CHECK_FLAG (rib->flags, ZEBRA_FLAG_IBGP)) 
	    {
	      rib_cnt[ZEBRA_ROUTE_IBGP]++;
	      if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB)) 
		fib_cnt[ZEBRA_ROUTE_IBGP]++;
	    }
	}

  vty_out (vty, "%-20s %-20s %-20s %s", 
	   "Route Source", "Routes", "FIB", VTY_NEWLINE);

  for (i = 0; i < ZEBRA_ROUTE_MAX; i++) 
    {
      if (rib_cnt[i] > 0)
	{
	  if (i == ZEBRA_ROUTE_BGP)
	    {
	      vty_out (vty, "%-20s %-20d %-20d %s", "ebgp", 
		       rib_cnt[ZEBRA_ROUTE_BGP] - rib_cnt[ZEBRA_ROUTE_IBGP],
		       fib_cnt[ZEBRA_ROUTE_BGP] - fib_cnt[ZEBRA_ROUTE_IBGP],
		       VTY_NEWLINE);
	      vty_out (vty, "%-20s %-20d %-20d %s", "ibgp", 
		       rib_cnt[ZEBRA_ROUTE_IBGP], fib_cnt[ZEBRA_ROUTE_IBGP],
		       VTY_NEWLINE);
	    }
	  else 
	    vty_out (vty, "%-20s %-20d %-20d %s", zebra_route_string(i), 
		     rib_cnt[i], fib_cnt[i], VTY_NEWLINE);
	}
    }

  vty_out (vty, "------%s", VTY_NEWLINE);
  vty_out (vty, "%-20s %-20d %-20d %s", "Totals", rib_cnt[ZEBRA_ROUTE_TOTAL], 
	   fib_cnt[ZEBRA_ROUTE_TOTAL], VTY_NEWLINE);  
}

/* Show route summary.  */
DEFUN (show_ip_route_summary,
       show_ip_route_summary_cmd,
       "show ip route summary",
       SHOW_STR
       IP_STR
       "IP routing table\n"
       "Summary of all routes\n")
{
  struct route_table *table;

  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  vty_show_ip_route_summary (vty, table);

  return CMD_SUCCESS;
}

/* Write static route configuration. */
static int
static_config (struct vty *vty, afi_t afi)
{
  struct route_node *rn;
  struct static_route *si;  
  struct route_table *stable;
  int write;
  
  write = 0;

  /* Lookup table.  */
  stable = vrf_static_table (afi, SAFI_UNICAST, 0);
  if (! stable)
    return -1;

  for (rn = route_top (stable); rn; rn = route_next (rn))
    for (si = rn->info; si; si = si->next)
      {
        char buf[INET6_ADDRSTRLEN];
        prefix2str (&rn->p, buf, sizeof(buf));
        
        vty_out (vty, "%s route %s",
                 (afi == AFI_IP ? "ip" : "ipv6"),
                 buf);
        
        if (si->flags)
          {
            switch (si->flags)
              {
                case ZEBRA_FLAG_BLACKHOLE:
                  vty_out (vty, " blackhole");
                  break;
                case ZEBRA_FLAG_REJECT:
                  vty_out (vty, " reject");
                  break;
                default:
                  assert ("Unsupported flag in static route!" == NULL);
              }
          }
        else
          {
            char buf[INET6_ADDRSTRLEN];
            
            if (si->gate)
              {
                inet_ntop (si->gate->family, &si->gate->u.prefix,
                           buf, sizeof(buf));
                vty_out (vty, " %s", buf);
              }
            
            if (si->ifname)
              vty_out (vty, " %s", si->ifname);
          }
              
        if (si->distance != ZEBRA_STATIC_DISTANCE_DEFAULT)
          vty_out (vty, " %d", si->distance);

        vty_out (vty, "%s", VTY_NEWLINE);

        write = 1;
      }
  return write;
}

DEFUN (show_ip_protocol,
       show_ip_protocol_cmd,
       "show ip protocol",
        SHOW_STR
        IP_STR
       "IP protocol filtering status\n")
{
    int i; 

    vty_out(vty, "Protocol    : route-map %s", VTY_NEWLINE);
    vty_out(vty, "------------------------%s", VTY_NEWLINE);
    for (i=0;i<ZEBRA_ROUTE_MAX;i++)
    {
        if (proto_rm[AFI_IP][i])
          vty_out (vty, "%-10s  : %-10s%s", zebra_route_string(i),
					proto_rm[AFI_IP][i],
					VTY_NEWLINE);
        else
          vty_out (vty, "%-10s  : none%s", zebra_route_string(i), VTY_NEWLINE);
    }
    if (proto_rm[AFI_IP][i])
      vty_out (vty, "%-10s  : %-10s%s", "any", proto_rm[AFI_IP][i],
					VTY_NEWLINE);
    else
      vty_out (vty, "%-10s  : none%s", "any", VTY_NEWLINE);

    return CMD_SUCCESS;
}


#ifdef HAVE_IPV6
DEFUN (ipv6_route,
       ipv6_route_cmd,
       "ipv6 route X:X::X:X/M (X:X::X:X|INTERFACE) [<1-255>]",
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "IPv6 gateway address\n"
       "IPv6 gateway interface name\n"
       "Distance value for this prefix\n")
{
  const short distpos = 2;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 1, argv[0], NULL, argv[1], NULL, NULL, dist);
}

DEFUN (ipv6_route_flags,
       ipv6_route_flags_cmd,
       "ipv6 route X:X::X:X/M (reject|blackhole) [<1-255>]",
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "Emit an ICMP unreachable when matched\n"
       "Silently discard pkts when matched\n"
       "Distance value for this prefix\n")
{
  const short distpos = 2;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
    
  return zebra_static (vty, 1, argv[0], NULL, NULL, NULL, argv[1], dist);
}

DEFUN (ipv6_route_ifname,
       ipv6_route_ifname_cmd,
       "ipv6 route X:X::X:X/M X:X::X:X INTERFACE [<1-255>]",
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "IPv6 gateway address\n"
       "IPv6 gateway interface name\n"
       "Distance value for this prefix\n")
{
  const short distpos = 3;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 1, argv[0], NULL, argv[1], argv[2], NULL, dist);
}

DEFUN (no_ipv6_route,
       no_ipv6_route_cmd,
       "no ipv6 route X:X::X:X/M (X:X::X:X|INTERFACE) [<1-255>]",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "IPv6 gateway address\n"
       "IPv6 gateway interface name\n"
       "Distance value for this prefix\n")
{
  const short distpos = 2;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 0, argv[0], NULL, argv[1], NULL, NULL, dist);
}

DEFUN (no_ipv6_route_ifname,
       no_ipv6_route_ifname_cmd,
       "no ipv6 route X:X::X:X/M X:X::X:X INTERFACE [<1-255>]",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "IPv6 gateway address\n"
       "IPv6 gateway interface name\n")
{
  const short distpos = 3;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 0, argv[0], NULL, argv[1], argv[2], NULL, dist);
}

DEFUN (no_ipv6_route_flags,
       no_ipv6_route_flags_cmd,
       "no ipv6 route X:X::X:X/M (reject|blackhole) [<1-255>]",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "Emit an ICMP unreachable when matched\n"
       "Silently discard pkts when matched\n"
       "Distance value for this prefix\n")
{
  const short distpos = 2;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  return zebra_static (vty, 0, argv[0], NULL, argv[1], NULL, argv[2], dist);
}

#define SHOW_ROUTE_V6_HEADER "Codes: K - kernel route, C - connected, S - static, R - RIPng, O - OSPFv3,%s       I - ISIS, B - BGP, * - FIB route.%s%s"

DEFUN (show_ipv6_route,
       show_ipv6_route_cmd,
       "show ipv6 route",
       SHOW_STR
       IP_STR
       "IPv6 routing table\n")
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  int first = 1;

  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  /* Show all IPv6 route. */
  for (rn = route_top (table); rn; rn = route_next (rn))
    for (rib = rn->info; rib; rib = rib->next)
      {
	if (first)
	  {
	    vty_out (vty, SHOW_ROUTE_V6_HEADER, VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE);
	    first = 0;
	  }
	vty_show_ip_route (vty, rn, rib);
      }
  return CMD_SUCCESS;
}

DEFUN (show_ipv6_route_prefix_longer,
       show_ipv6_route_prefix_longer_cmd,
       "show ipv6 route X:X::X:X/M longer-prefixes",
       SHOW_STR
       IP_STR
       "IPv6 routing table\n"
       "IPv6 prefix\n"
       "Show route matching the specified Network/Mask pair only\n")
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  struct prefix p;
  int ret;
  int first = 1;

  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  ret = str2prefix (argv[0], &p);
  if (! ret)
    {
      vty_out (vty, "%% Malformed Prefix%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Show matched type IPv6 routes. */
  for (rn = route_top (table); rn; rn = route_next (rn))
    for (rib = rn->info; rib; rib = rib->next)
      if (prefix_match (&p, &rn->p))
	{
	  if (first)
	    {
	      vty_out (vty, SHOW_ROUTE_V6_HEADER, VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE);
	      first = 0;
	    }
	  vty_show_ip_route (vty, rn, rib);
	}
  return CMD_SUCCESS;
}

DEFUN (show_ipv6_route_protocol,
       show_ipv6_route_protocol_cmd,
       "show ipv6 route (bgp|connected|isis|kernel|ospf6|ripng|static)",
       SHOW_STR
       IP_STR
       "IP routing table\n"
       "Border Gateway Protocol (BGP)\n"
       "Connected\n"
       "ISO IS-IS (ISIS)\n"
       "Kernel\n"
       "Open Shortest Path First (OSPFv3)\n"
       "Routing Information Protocol (RIPng)\n"
       "Static routes\n")
{
  zebra_route_t type;
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  int first = 1;

  if (strncmp (argv[0], "b", 1) == 0)
    type = ZEBRA_ROUTE_BGP;
  else if (strncmp (argv[0], "c", 1) == 0)
    type = ZEBRA_ROUTE_CONNECT;
  else if (strncmp (argv[0], "k", 1) ==0)
    type = ZEBRA_ROUTE_KERNEL;
  else if (strncmp (argv[0], "o", 1) == 0)
    type = ZEBRA_ROUTE_OSPF6;
  else if (strncmp (argv[0], "i", 1) == 0)
    type = ZEBRA_ROUTE_ISIS;
  else if (strncmp (argv[0], "r", 1) == 0)
    type = ZEBRA_ROUTE_RIPNG;
  else if (strncmp (argv[0], "s", 1) == 0)
    type = ZEBRA_ROUTE_STATIC;
  else 
    {
      vty_out (vty, "Unknown route type%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  
  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  /* Show matched type IPv6 routes. */
  for (rn = route_top (table); rn; rn = route_next (rn))
    for (rib = rn->info; rib; rib = rib->next)
      if (rib->type == type)
	{
	  if (first)
	    {
	      vty_out (vty, SHOW_ROUTE_V6_HEADER, VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE);
	      first = 0;
	    }
	  vty_show_ip_route (vty, rn, rib);
	}
  return CMD_SUCCESS;
}

DEFUN (show_ipv6_route_addr,
       show_ipv6_route_addr_cmd,
       "show ipv6 route X:X::X:X",
       SHOW_STR
       IP_STR
       "IPv6 routing table\n"
       "IPv6 Address\n")
{
  int ret;
  struct prefix_ipv6 p;
  struct route_table *table;
  struct route_node *rn;

  ret = str2prefix_ipv6 (argv[0], &p);
  if (ret <= 0)
    {
      vty_out (vty, "Malformed IPv6 address%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  rn = route_node_match (table, (struct prefix *) &p);
  if (! rn)
    {
      vty_out (vty, "%% Network not in table%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  vty_show_ip_route_detail (vty, rn);

  route_unlock_node (rn);

  return CMD_SUCCESS;
}

DEFUN (show_ipv6_route_prefix,
       show_ipv6_route_prefix_cmd,
       "show ipv6 route X:X::X:X/M",
       SHOW_STR
       IP_STR
       "IPv6 routing table\n"
       "IPv6 prefix\n")
{
  int ret;
  struct prefix_ipv6 p;
  struct route_table *table;
  struct route_node *rn;

  ret = str2prefix_ipv6 (argv[0], &p);
  if (ret <= 0)
    {
      vty_out (vty, "Malformed IPv6 prefix%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  rn = route_node_match (table, (struct prefix *) &p);
  if (! rn || rn->p.prefixlen != p.prefixlen)
    {
      vty_out (vty, "%% Network not in table%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  vty_show_ip_route_detail (vty, rn);

  route_unlock_node (rn);

  return CMD_SUCCESS;
}

/* Show route summary.  */
DEFUN (show_ipv6_route_summary,
       show_ipv6_route_summary_cmd,
       "show ipv6 route summary",
       SHOW_STR
       IP_STR
       "IPv6 routing table\n"
       "Summary of all IPv6 routes\n")
{
  struct route_table *table;

  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return CMD_SUCCESS;

  vty_show_ip_route_summary (vty, table);

  return CMD_SUCCESS;
}
#endif /* HAVE_IPV6 */

/* Static ip route configuration write function. */
static int
zebra_ip_config (struct vty *vty)
{
  int write = 0;

  write += static_config (vty, AFI_IP);
#ifdef HAVE_IPV6
  write += static_config (vty, AFI_IP6);
#endif /* HAVE_IPV6 */

  return write;
}

/* ip protocol configuration write function */
static int config_write_protocol(struct vty *vty)
{  
  int i;

  for (i=0;i<ZEBRA_ROUTE_MAX;i++)
    {
      if (proto_rm[AFI_IP][i])
        vty_out (vty, "ip protocol %s route-map %s%s", zebra_route_string(i),
                 proto_rm[AFI_IP][i], VTY_NEWLINE);
    }
  if (proto_rm[AFI_IP][ZEBRA_ROUTE_MAX])
      vty_out (vty, "ip protocol %s route-map %s%s", "any",
               proto_rm[AFI_IP][ZEBRA_ROUTE_MAX], VTY_NEWLINE);

  return 1;
}   

/* table node for protocol filtering */
static struct cmd_node protocol_node = { PROTOCOL_NODE, "", 1 };

/* IP node for static routes. */
static struct cmd_node ip_node = { IP_NODE,  "",  1 };

#include "zebra_old_cmds.c"

/* Route VTY.  */
void
zebra_vty_init (void)
{
  install_node (&ip_node, zebra_ip_config);
  install_node (&protocol_node, config_write_protocol);

  install_element (CONFIG_NODE, &ip_protocol_cmd);
  install_element (CONFIG_NODE, &no_ip_protocol_cmd);
  install_element (VIEW_NODE, &show_ip_protocol_cmd);
  install_element (ENABLE_NODE, &show_ip_protocol_cmd);
  install_element (CONFIG_NODE, &ip_route_cmd);
  install_element (CONFIG_NODE, &ip_route_flags_cmd);
  install_element (CONFIG_NODE, &no_ip_route_cmd);
  install_element (CONFIG_NODE, &no_ip_route_flags_cmd);

  install_element (VIEW_NODE, &show_ip_route_cmd);
  install_element (VIEW_NODE, &show_ip_route_addr_cmd);
  install_element (VIEW_NODE, &show_ip_route_prefix_cmd);
  install_element (VIEW_NODE, &show_ip_route_prefix_longer_cmd);
  install_element (VIEW_NODE, &show_ip_route_protocol_cmd);
  install_element (VIEW_NODE, &show_ip_route_supernets_cmd);
  install_element (VIEW_NODE, &show_ip_route_summary_cmd);
  install_element (ENABLE_NODE, &show_ip_route_cmd);
  install_element (ENABLE_NODE, &show_ip_route_addr_cmd);
  install_element (ENABLE_NODE, &show_ip_route_prefix_cmd);
  install_element (ENABLE_NODE, &show_ip_route_prefix_longer_cmd);
  install_element (ENABLE_NODE, &show_ip_route_protocol_cmd);
  install_element (ENABLE_NODE, &show_ip_route_supernets_cmd);
  install_element (ENABLE_NODE, &show_ip_route_summary_cmd);

#ifdef HAVE_IPV6
  install_element (CONFIG_NODE, &ipv6_route_cmd);
  install_element (CONFIG_NODE, &ipv6_route_flags_cmd);
  install_element (CONFIG_NODE, &ipv6_route_ifname_cmd);
  install_element (CONFIG_NODE, &no_ipv6_route_cmd);
  install_element (CONFIG_NODE, &no_ipv6_route_flags_cmd);
  install_element (CONFIG_NODE, &no_ipv6_route_ifname_cmd);
  install_element (VIEW_NODE, &show_ipv6_route_cmd);
  install_element (VIEW_NODE, &show_ipv6_route_summary_cmd);
  install_element (VIEW_NODE, &show_ipv6_route_protocol_cmd);
  install_element (VIEW_NODE, &show_ipv6_route_addr_cmd);
  install_element (VIEW_NODE, &show_ipv6_route_prefix_cmd);
  install_element (VIEW_NODE, &show_ipv6_route_prefix_longer_cmd);
  install_element (ENABLE_NODE, &show_ipv6_route_cmd);
  install_element (ENABLE_NODE, &show_ipv6_route_protocol_cmd);
  install_element (ENABLE_NODE, &show_ipv6_route_addr_cmd);
  install_element (ENABLE_NODE, &show_ipv6_route_prefix_cmd);
  install_element (ENABLE_NODE, &show_ipv6_route_prefix_longer_cmd);
  install_element (ENABLE_NODE, &show_ipv6_route_summary_cmd);
#endif /* HAVE_IPV6 */

  zebra_vty_old_init ();
}

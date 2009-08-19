/* Commands from zebra_vty.c that are deprecated, for #include'ing into same
 * file
 *
 * Deprecated:
 *
 * - Specifying prefixes by IP and Mask
 * - Specifying a static route with both discard/reject flags + a gateway
 *   as its non-sensical.
 */
DEFUN_DEPRECATED (ip_route_gate_flags,
       ip_route_gate_flags_cmd,
       "ip route A.B.C.D/M (A.B.C.D|INTERFACE) (reject|blackhole) [<1-255>]",
       IP_STR
       "Establish static routes\n"
       "IP destination prefix (e.g. 10.0.0.0/8)\n"
       "IP gateway address\n"
       "IP gateway interface name\n"
       "Emit an ICMP unreachable when matched\n"
       "Silently discard pkts when matched\n"
       "Distance value for this route\n")
{
  const short distpos = 3;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 1, argv[0], NULL, NULL, NULL, argv[2], dist);
}

/* Mask as A.B.C.D format.  */
DEFUN_DEPRECATED (ip_route_mask,
       ip_route_mask_cmd,
       "ip route A.B.C.D A.B.C.D (A.B.C.D|INTERFACE|null0) [<1-255>]",
       IP_STR
       "Establish static routes\n"
       "IP destination prefix\n"
       "IP destination prefix mask\n"
       "IP gateway address\n"
       "IP gateway interface name\n"
       "Null interface\n"
       "Distance value for this route\n")
{
  const short distpos = 3;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 1, argv[0], argv[1], argv[2], NULL, NULL, dist);
}

DEFUN_DEPRECATED (ip_route_mask_gate_flags,
       ip_route_mask_gate_flags_cmd,
       "ip route A.B.C.D A.B.C.D (A.B.C.D|INTERFACE) (reject|blackhole) [<1-255>]",
       IP_STR
       "Establish static routes\n"
       "IP destination prefix\n"
       "IP destination prefix mask\n"
       "IP gateway address\n"
       "IP gateway interface name\n"
       "Emit an ICMP unreachable when matched\n"
       "Silently discard pkts when matched\n"
       "Distance value for this route\n")
{
  const short distpos = 4;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 1, argv[0], argv[1], NULL, NULL, argv[3], dist);
}

DEFUN_DEPRECATED (ip_route_mask_flags,
       ip_route_mask_flags_cmd,
       "ip route A.B.C.D A.B.C.D (reject|blackhole) [<1-255>]",
       IP_STR
       "Establish static routes\n"
       "IP destination prefix\n"
       "IP destination prefix mask\n"
       "Emit an ICMP unreachable when matched\n"
       "Silently discard pkts when matched\n"
       "Distance value for this route\n")
{
  const short distpos = 3;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 1, argv[0], argv[1], NULL, NULL, argv[2], dist);
}

DEFUN_DEPRECATED (no_ip_route_mask,
       no_ip_route_mask_cmd,
       "no ip route A.B.C.D A.B.C.D (A.B.C.D|INTERFACE|null0) [<1-255>]",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IP destination prefix\n"
       "IP destination prefix mask\n"
       "IP gateway address\n"
       "IP gateway interface name\n"
       "Null interface\n"
       "Distance value for this route\n")
{
  const short distpos = 3;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 0, argv[0], argv[1], argv[2], NULL, NULL, dist);
}

DEFUN_DEPRECATED (no_ip_route_mask_gate_flags,
       no_ip_route_mask_gate_flags_cmd,
       "no ip route A.B.C.D A.B.C.D (A.B.C.D|INTERFACE) (reject|blackhole) [<1-255>]",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IP destination prefix\n"
       "IP destination prefix mask\n"
       "IP gateway address\n"
       "IP gateway interface name\n"
       "Emit an ICMP unreachable when matched\n"
       "Silently discard pkts when matched\n"
       "Distance value for this route\n")
{
  const short distpos = 4;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 0, argv[0], argv[1], NULL, NULL, argv[3], dist);
}
DEFUN_DEPRECATED (no_ip_route_mask_flags,
       no_ip_route_mask_flags_cmd,
       "no ip route A.B.C.D A.B.C.D (reject|blackhole) [<1-255>]",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IP destination prefix\n"
       "IP destination prefix mask\n"
       "Emit an ICMP unreachable when matched\n"
       "Silently discard pkts when matched\n"
       "Distance value for this route\n")
{
  const short distpos = 3;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 0, argv[0], argv[1], NULL, NULL, argv[2], dist);
}

DEFUN_DEPRECATED (no_ip_route_gate_flags,
       no_ip_route_gate_flags_cmd,
       "no ip route A.B.C.D/M (A.B.C.D|INTERFACE) (reject|blackhole) [<1-255>]",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IP destination prefix (e.g. 10.0.0.0/8)\n"
       "IP gateway address\n"
       "IP gateway interface name\n"
       "Emit an ICMP unreachable when matched\n"
       "Silently discard pkts when matched\n"
       "Distance value for this route\n")
{
  const short distpos = 3;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 0, argv[0], NULL, NULL, NULL, argv[2], dist);
}

#ifdef HAVE_IPV6
DEFUN_DEPRECATED (ipv6_route_gate_flags,
       ipv6_route_gate_flags_cmd,
       "ipv6 route X:X::X:X/M (X:X::X:X|INTERFACE) (reject|blackhole) [<1-255>]",
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "IPv6 gateway address\n"
       "IPv6 gateway interface name\n"
       "Emit an ICMP unreachable when matched\n"
       "Silently discard pkts when matched\n"
       "Distance value for this route\n")
{
  const short distpos = 3;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 1, argv[0], NULL, NULL, NULL, argv[2], dist);
}

DEFUN_DEPRECATED (ipv6_route_gate_ifname_flags,
       ipv6_route_gate_ifname_flags_cmd,
       "ipv6 route X:X::X:X/M X:X::X:X INTERFACE (reject|blackhole) [<1-255>]",
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "IPv6 gateway address\n"
       "IPv6 gateway interface name\n"
       "Emit an ICMP unreachable when matched\n"
       "Silently discard pkts when matched\n"
       "Distance value for this route\n")
{
  const short distpos = 4;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 1, argv[0], NULL, NULL, NULL, argv[3], dist);
}

DEFUN_DEPRECATED (no_ipv6_route_gate_flags,
       no_ipv6_route_gate_flags_cmd,
       "no ipv6 route X:X::X:X/M (X:X::X:X|INTERFACE) (reject|blackhole) [<1-255>]",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "IPv6 gateway address\n"
       "IPv6 gateway interface name\n"
       "Emit an ICMP unreachable when matched\n"
       "Silently discard pkts when matched\n"
       "Distance value for this route\n")
{
  const short distpos = 3;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 1, argv[0], NULL, NULL, NULL, argv[2], dist);
}

DEFUN_DEPRECATED (no_ipv6_route_gate_ifname_flags,
       no_ipv6_route_gate_ifname_flags_cmd,
       "no ipv6 route X:X::X:X/M X:X::X:X INTERFACE (reject|blackhole) [<1-255>]",
       NO_STR
       IP_STR
       "Establish static routes\n"
       "IPv6 destination prefix (e.g. 3ffe:506::/32)\n"
       "IPv6 gateway address\n"
       "IPv6 gateway interface name\n"
       "Emit an ICMP unreachable when matched\n"
       "Silently discard pkts when matched\n"
       "Distance value for this route\n")
{
  const short distpos = 4;
  const char *dist = (argc == (distpos + 1) ? argv[distpos] : NULL);
  
  return zebra_static (vty, 1, argv[0], NULL, NULL, NULL, argv[3], dist);
}
#endif /* HAVE_IPV6 */

static void
zebra_vty_old_init (void)
{
  install_element (CONFIG_NODE, &ip_route_gate_flags_cmd);
  install_element (CONFIG_NODE, &ip_route_mask_cmd);
  install_element (CONFIG_NODE, &ip_route_mask_gate_flags_cmd);
  install_element (CONFIG_NODE, &ip_route_mask_flags_cmd);
  install_element (CONFIG_NODE, &no_ip_route_mask_cmd);
  install_element (CONFIG_NODE, &no_ip_route_mask_gate_flags_cmd);
  install_element (CONFIG_NODE, &no_ip_route_mask_flags_cmd);
  install_element (CONFIG_NODE, &no_ip_route_gate_flags_cmd);
  
#ifdef HAVE_IPV6
  install_element (CONFIG_NODE, &ipv6_route_gate_flags_cmd);
  install_element (CONFIG_NODE, &ipv6_route_gate_ifname_flags_cmd);
  install_element (CONFIG_NODE, &no_ipv6_route_gate_flags_cmd);
  install_element (CONFIG_NODE, &no_ipv6_route_gate_ifname_flags_cmd);
#endif /* HAVE_IPV6 */
}

/* Community attribute related functions.
   Copyright (C) 1998, 2001 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include <zebra.h>

#include "hash.h"
#include "memory.h"
#include "object.h"

#include "bgpd/bgp_community.h"

/* community attribute object context. */
static const struct object_ctx *comobj_ctx;

static struct community *
community_new (void)
{
  return object_new (comobj_ctx);
}

/* Free communities value.  */
static void
community_clean (void *v)
{
  struct community *com = v;
  if (com->val)
    XFREE (MTYPE_COMMUNITY_VAL, com->val);
  if (com->str)
    XFREE (MTYPE_COMMUNITY_STR, com->str);
}

void
community_free (struct community *com)
{
  community_clean (com);
  XFREE (MTYPE_COMMUNITY, com);
}

/* Add one community value to the community. */
static void
community_add_val (struct community *com, u_int32_t val)
{
  com->size++;
  if (com->val)
    com->val = XREALLOC (MTYPE_COMMUNITY_VAL, com->val, com_length (com));
  else
    com->val = XMALLOC (MTYPE_COMMUNITY_VAL, com_length (com));

  val = htonl (val);
  memcpy (com_lastval (com), &val, sizeof (u_int32_t));
}

/* Delete one community. */
void
community_del_val (struct community *com, u_int32_t *val)
{
  int i = 0;
  int c = 0;

  if (! com->val)
    return;

  while (i < com->size)
    {
      if (memcmp (com->val + i, val, sizeof (u_int32_t)) == 0)
	{
	  c = com->size -i -1;

	  if (c > 0)
	    memcpy (com->val + i, com->val + (i + 1), c * sizeof (*val));

	  com->size--;

	  if (com->size > 0)
	    com->val = XREALLOC (MTYPE_COMMUNITY_VAL, com->val,
				 com_length (com));
	  else
	    {
	      XFREE (MTYPE_COMMUNITY_VAL, com->val);
	      com->val = NULL;
	    }
	  return;
	}
      i++;
    }
}

/* Delete all communities listed in com2 from com1 */
struct community *
community_delete (struct community *com1, struct community *com2)
{
  int i = 0;

  while(i < com2->size)
    {
      community_del_val (com1, com2->val + i);
      i++;
    }

  return com1;
}

/* Callback function from qsort(). */
static int
community_compare (const void *a1, const void *a2)
{
  u_int32_t v1;
  u_int32_t v2;

  memcpy (&v1, a1, sizeof (u_int32_t));
  memcpy (&v2, a2, sizeof (u_int32_t));
  v1 = ntohl (v1);
  v2 = ntohl (v2);

  if (v1 < v2)
    return -1;
  if (v1 > v2)
    return 1;
  return 0;
}

int
community_include (const struct community *com, u_int32_t val)
{
  int i;

  val = htonl (val);

  for (i = 0; i < com->size; i++)
    if (memcmp (&val, com_nthval (com, i), sizeof (u_int32_t)) == 0)
      return 1;

  return 0;
}

static u_int32_t
community_val_get (const struct community *com, int i)
{
  u_char *p = (u_char *) com->val;
  u_int32_t val;

  p += (i * 4);

  memcpy (&val, p, sizeof (u_int32_t));

  return ntohl (val);
}

static void
community_val_set (const struct community *com, u_int32_t val, int i)
{
  u_char *p;
  u_int32_t lav = htonl (val);

  p = (u_char *) com->val;
  p += (i * 4);

  memcpy (p, &lav, sizeof (u_int32_t));
}


/* Sort and uniq given community. */
static void
community_uniq_sort (struct community *com)
{
  int i, tail;
  u_int32_t val;
  
  if (!com)
    return;
  
  if (com->size == 0)
    return;
  
  qsort (com->val, com->size, sizeof (u_int32_t), community_compare);
  
  for (i = 1, tail = 0; i < com->size; i++)
    {
      val = community_val_get (com, i);
      
      if (community_val_get (com, tail) == val)
        continue;
      
      tail++;
      
      if ((i - tail) == 1)
        continue;
      
      community_val_set (com, val, tail);
    }
  
  com->size = tail + 1;
  
  return;
}

/* Convert communities attribute to string.

   For Well-known communities value, below keyword is used.

   0x0             "internet"    
   0xFFFFFF01      "no-export"
   0xFFFFFF02      "no-advertise"
   0xFFFFFF03      "local-AS"

   For other values, "AS:VAL" format is used.  */
static char *
community_com2str  (const struct community *const com)
{
  int i;
  char *str;
  char *pnt;
  int len;
  int first;
  u_int32_t comval;
  u_int16_t as;
  u_int16_t val;

  if (!com)
    return NULL;
  
  /* When communities attribute is empty.  */
  if (com->size == 0)
    {
      str = XMALLOC (MTYPE_COMMUNITY_STR, 1);
      str[0] = '\0';
      return str;
    }

  /* Memory allocation is time consuming work.  So we calculate
     required string length first.  */
  len = 0;

  for (i = 0; i < com->size; i++)
    {
      memcpy (&comval, com_nthval (com, i), sizeof (u_int32_t));
      comval = ntohl (comval);

      switch (comval) 
	{
	case COMMUNITY_INTERNET:
	  len += strlen (" internet");
	  break;
	case COMMUNITY_NO_EXPORT:
	  len += strlen (" no-export");
	  break;
	case COMMUNITY_NO_ADVERTISE:
	  len += strlen (" no-advertise");
	  break;
	case COMMUNITY_LOCAL_AS:
	  len += strlen (" local-AS");
	  break;
	default:
	  len += strlen (" 65536:65535");
	  break;
	}
    }

  /* Allocate memory.  */
  str = pnt = XMALLOC (MTYPE_COMMUNITY_STR, len);
  first = 1;

  /* Fill in string.  */
  for (i = 0; i < com->size; i++)
    {
      memcpy (&comval, com_nthval (com, i), sizeof (u_int32_t));
      comval = ntohl (comval);

      if (first)
	first = 0;
      else
	*pnt++ = ' ';

      switch (comval) 
	{
	case COMMUNITY_INTERNET:
	  strcpy (pnt, "internet");
	  pnt += strlen ("internet");
	  break;
	case COMMUNITY_NO_EXPORT:
	  strcpy (pnt, "no-export");
	  pnt += strlen ("no-export");
	  break;
	case COMMUNITY_NO_ADVERTISE:
	  strcpy (pnt, "no-advertise");
	  pnt += strlen ("no-advertise");
	  break;
	case COMMUNITY_LOCAL_AS:
	  strcpy (pnt, "local-AS");
	  pnt += strlen ("local-AS");
	  break;
	default:
	  as = (comval >> 16) & 0xFFFF;
	  val = comval & 0xFFFF;
	  sprintf (pnt, "%u:%d", as, val);
	  pnt += strlen (pnt);
	  break;
	}
    }
  *pnt = '\0';

  return str;
}

const struct community *
community_ref (struct community *com)
{
  return object_ref (com);
}

void
community_deref (struct community **com)
{
  object_deref (*com);
  *com = NULL;
}

void
community_swap (struct community *old, struct community *com)
{
  object_ref_swap (old, com);
}

/* Create new community attribute. */
struct community *
community_parse (u_int32_t *pnt, u_short length)
{
  struct community *new;

  /* If length is malformed return NULL. */
  if (length % 4)
    return NULL;

  /* Make temporary community for hash look up. */
  new = object_new (comobj_ctx);
  new->val = XREALLOC (MTYPE_COMMUNITY_VAL, new->val, length);
  new->size = length / 4;
  memcpy (new->val, pnt, length);

  community_uniq_sort (new);

  return new;
}

struct community *
community_dup (const struct community *com)
{
  return object_dup (com);
}

static void *
community_dup_obj (void *dst, const void *src)
{
  struct community *new = dst;
  const struct community *com = src;
  
  new->size = com->size;
  if (new->size)
    {
      new->val = XMALLOC (MTYPE_COMMUNITY_VAL, com->size * 4);
      memcpy (new->val, com->val, com->size * 4);
    }
  else
    new->val = NULL;
  
  return new;
}

/* Retrun string representation of communities attribute. */
char *
community_str (struct community *com)
{
  if (!com)
    return NULL;
  
  if (! com->str)
    com->str = community_com2str (com);
  return com->str;
}

/* Make hash value of community attribute. This function is used by
   hash package.*/
static unsigned int
community_hash_make_obj (const void *v)
{
  return community_hash_make (v);
}

unsigned int
community_hash_make (const struct community *com)
{
  int c;
  unsigned int key;
  unsigned char *pnt;

  key = 0;
  pnt = (unsigned char *)com->val;
  
  for(c = 0; c < com->size * 4; c++)
    key += pnt[c];
      
  return key;
}

int
community_match (const struct community *com1, const struct community *com2)
{
  int i = 0;
  int j = 0;

  if (com1 == NULL && com2 == NULL)
    return 1;

  if (com1 == NULL || com2 == NULL)
    return 0;

  if (com1->size < com2->size)
    return 0;

  /* Every community on com2 needs to be on com1 for this to match */
  while (i < com1->size && j < com2->size)
    {
      if (memcmp (com1->val + i, com2->val + j, sizeof (u_int32_t)) == 0)
	j++;
      i++;
    }

  if (j == com2->size)
    return 1;
  else
    return 0;
}

/* If two aspath have same value then return 1 else return 0. This
   function is used by hash package. */
int
community_cmp (const struct community *com1, const struct community *com2)
{
  if (com1 == NULL && com2 == NULL)
    return 1;
  if (com1 == NULL || com2 == NULL)
    return 0;

  if (com1->size == com2->size)
    if (memcmp (com1->val, com2->val, com1->size * 4) == 0)
      return 1;
  return 0;
}

static bool
community_cmp_obj (const void *com1, const void *com2)
{
  return community_cmp (com1, com2) == 1 ? true : false;
}

/* Add com2 to the end of com1. */
struct community *
community_merge (struct community *com1, const struct community *com2)
{
  if (com1->val)
    com1->val = XREALLOC (MTYPE_COMMUNITY_VAL, com1->val, 
			  (com1->size + com2->size) * 4);
  else
    com1->val = XMALLOC (MTYPE_COMMUNITY_VAL, (com1->size + com2->size) * 4);

  memcpy (com1->val + com1->size, com2->val, com2->size * 4);
  com1->size += com2->size;
  
  community_uniq_sort (com1);
  
  return com1;
}

/* Community token enum. */
enum community_token
{
  community_token_val,
  community_token_no_export,
  community_token_no_advertise,
  community_token_local_as,
  community_token_unknown
};

/* Get next community token from string. */
static const char *
community_gettoken (const char *buf, enum community_token *token, 
                    u_int32_t *val)
{
  const char *p = buf;

  /* Skip white space. */
  while (isspace ((int) *p))
    p++;

  /* Check the end of the line. */
  if (*p == '\0')
    return NULL;

  /* Well known community string check. */
  if (isalpha ((int) *p)) 
    {
      if (strncmp (p, "internet", strlen ("internet")) == 0)
	{
	  *val = COMMUNITY_INTERNET;
	  *token = community_token_no_export;
	  p += strlen ("internet");
	  return p;
	}
      if (strncmp (p, "no-export", strlen ("no-export")) == 0)
	{
	  *val = COMMUNITY_NO_EXPORT;
	  *token = community_token_no_export;
	  p += strlen ("no-export");
	  return p;
	}
      if (strncmp (p, "no-advertise", strlen ("no-advertise")) == 0)
	{
	  *val = COMMUNITY_NO_ADVERTISE;
	  *token = community_token_no_advertise;
	  p += strlen ("no-advertise");
	  return p;
	}
      if (strncmp (p, "local-AS", strlen ("local-AS")) == 0)
	{
	  *val = COMMUNITY_LOCAL_AS;
	  *token = community_token_local_as;
	  p += strlen ("local-AS");
	  return p;
	}

      /* Unknown string. */
      *token = community_token_unknown;
      return NULL;
    }

  /* Community value. */
  if (isdigit ((int) *p)) 
    {
      int separator = 0;
      int digit = 0;
      u_int32_t community_low = 0;
      u_int32_t community_high = 0;

      while (isdigit ((int) *p) || *p == ':') 
	{
	  if (*p == ':') 
	    {
	      if (separator)
		{
		  *token = community_token_unknown;
		  return NULL;
		}
	      else
		{
		  separator = 1;
		  digit = 0;
		  community_high = community_low << 16;
		  community_low = 0;
		}
	    }
	  else 
	    {
	      digit = 1;
	      community_low *= 10;
	      community_low += (*p - '0');
	    }
	  p++;
	}
      if (! digit)
	{
	  *token = community_token_unknown;
	  return NULL;
	}
      *val = community_high + community_low;
      *token = community_token_val;
      return p;
    }
  *token = community_token_unknown;
  return NULL;
}

/* convert string to community structure */
struct community *
community_str2com (const char *str)
{
  struct community *com = NULL;
  u_int32_t val = 0;
  enum community_token token = community_token_unknown;

  do 
    {
      str = community_gettoken (str, &token, &val);
      
      switch (token)
	{
	case community_token_val:
	case community_token_no_export:
	case community_token_no_advertise:
	case community_token_local_as:
	  if (com == NULL)
	    com = community_new();
	  community_add_val (com, val);
	  break;
	case community_token_unknown:
	default:
	  if (com)
	    community_free (com);
	  return NULL;
	}
    } while (str);
  
  if (! com)
    return NULL;

  community_uniq_sort (com);

  return com;
}

/* Return communities hash entry count.  */
unsigned long
community_count (void)
{
  return object_num_cached (comobj_ctx);
}

/* Iterate over communities.  */
void
community_iterate (void (*func) (struct hash_backet *, void *), void *arg)
{
  object_iterate (comobj_ctx, func, arg);
}

/* Initialize community related hash. */
void
community_init (void)
{
  struct object_table tmp =
  {
    .size = sizeof (struct community),
    .finish = &community_clean,
    .equal = &community_cmp_obj,
    .hash_key = community_hash_make_obj,
    .dup = community_dup_obj,
    .memtype = MTYPE_COMMUNITY,
  };
  comobj_ctx = object_init (&tmp);
}

void
community_finish (void)
{
  object_finish (comobj_ctx);
}

/* Object management functions
 *
 * Copyright (C) 2011 Paul Jakma
 *
 * This file is part of Quagga.
 *
 * Quagga is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
 *
 * Quagga is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Quagga; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _QUAGGA_OBJECT_H
#define _QUAGGA_OBJECT_H

#include "hash.h"

/* opaque types */
struct object_ctx;
struct object;

struct object_table
{
  const int memtype;
  /* Size of the object specific data */
  const size_t size;
  /* initialise object specific data */
  void (*const init) (void *);
  /* cleanup any the object specific data */
  void (*const finish) (void *);
  /* duplicate the supplied data */
  void *(*const dup) (void *, const void *src);
  
  /* Semantic equality between different objects.
   * Objects equal in this way are considered fully interchangeable.
   */
  bool (*const equal) (const void *, const void *);
  unsigned int (*const hash_key) (const void *);
};

/* Create/destroy an object context */
extern const struct object_ctx *object_init (const struct object_table *);
extern void object_finish (const struct object_ctx *);

extern unsigned long object_num_cached (const struct object_ctx *);
extern unsigned long object_refcnt (void *);

/* Create a new object */
extern void *object_new (const struct object_ctx *);

/* users may want to wrap these further */
extern const void *object_ref (void *ref);
extern void object_deref (void *ref);
#define OBJECT_DEREF(R) (object_deref (R), (R) = NULL)
extern void object_ref_swap (void *old, void *ref);

/* get a floating, uncached reference to a duplicate of the object */
extern void *object_dup (const void *);

/* get an uncached, mutable version of the object.
 * If the object is already floating/uncached, then the same
 * reference is returned, otherwise the object is duplicated.
 */
extern void *object_get_mutable (const void *);

extern void object_iterate (const struct object_ctx *,
                            void (*) (struct hash_backet *, void *),
                            void *);

/* Usually shouldn't be needed outside of object.c */
extern void *object_to_data (const struct object *);
extern struct object *object_from_data (void *);

#endif /* _QUAGGA_OBJECT_H */

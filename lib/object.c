/* A lightweight object management layer
 *
 * Copyright (C) 2012 Paul Jakma
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
#include <zebra.h>
#include <memory.h>
#include <hash.h>

#include <object.h>

/* Global context for an object type */
struct object_ctx
{
  const struct object_table t;
  struct hash *const h;
};

/* Per-object header, prepended to the allocated memory.
 * Keep this as small as possible, and watch alignment.
 */
struct object
{
  unsigned long refcnt;
  const struct object_ctx *const ctx;
  void *const data[];
};

const struct object_ctx *
object_init (const struct object_table *t)
{
  struct object_ctx *ctx;
  struct hash *h;
  
  assert (t != NULL);
  assert (t->init != NULL);
  assert (t->finish != NULL);
  
  if (t == NULL || t->init == NULL || t->finish == NULL)
    return NULL;
  
  ctx = XCALLOC (MTYPE_OBJ_CTX, sizeof (struct object_ctx));
  memcpy ((struct object_table *)(&ctx->t), t, sizeof (struct object_table));
  
  if (t->hash_key)
    {
      h = hash_create ((unsigned int (*) (void *))t->hash_key,
                       (int (*) (const void *, const void *))t->equal);
      memcpy ((struct hash *)(ctx->h), h, sizeof (struct hash *));
    }
  
  return ctx;
}

void
object_finish (const struct object_ctx *ctx)
{
  XFREE (MTYPE_OBJ_CTX, ctx);
}

void *
object_new (const struct object_ctx *ctx)
{
  struct object *obj;
  
  assert (ctx != NULL);
  
  if (ctx == NULL)
    return NULL;
  
  obj = XCALLOC (ctx->t.memtype,
                 sizeof (struct object) + ctx->t.size);
  
  obj->ctx->t.init (obj->data[0]);
  
  return object_to_data (obj);
}

static void
object_free (struct object *obj)
{
  obj->ctx->t.finish (obj->data[0]);
  XFREE (obj->ctx->t.memtype, obj);
}

static struct object *
object_cache (struct object *obj)
{
  struct object *exist;
  exist = object_from_data (hash_get (obj->ctx->h, obj->data[0],
                                      hash_alloc_intern));
  
  if (exist != obj)
    obj->ctx->t.finish (obj);
  
  return exist;
}

static void
object_decache (struct object *object)
{
  hash_release (object->ctx->h, object->data[0]);
}

void *
object_ref (void *o)
{
  struct object *obj;
  
  if (o == NULL)
    return NULL;
  
  obj = object_from_data (o);
  if (obj->ctx->t.hash_key)
    obj = object_cache (obj);
  
  obj->refcnt++;
  
  return obj->data[0];
}

void
object_deref (void *ref)
{
  struct object *obj = object_from_data (ref);
  assert (obj->refcnt > 0);
  
  obj->refcnt--;
  
  if (obj->refcnt == 0)
    {
      if (obj->ctx->t.hash_key)
        object_decache (obj);
      object_free (obj);
    }
}

void
object_ref_swap (void *ref, void *obj)
{
  if (ref != NULL)
    object_deref (ref);
  
  if (obj)
    ref = object_ref (obj);
}

/* internal helper for duping objects */
static void *
object_dup_obj (const struct object *obj)
{
  if (!obj->ctx->t.dup)
    return NULL;
  
  dup = object_new (obj->ctx);
  obj->ctx->t.dup (dup, obj->data[0]);
  
  return dup;
}

void *
object_dup (const void *ref)
{
  struct object *obj = object_from_data (*((char **)ref));
  struct object *dup;
  
  return object_dup_obj (obj);
}

void *
object_get_mutable (object *data)
{
  struct object *obj = object_from_data (data);
  
  if (obj->refcnt == 0)
    return data;
    
  return object_dup_obj (obj);
}

unsigned long
object_refcnt (void *data)
{
  return object_from_data (data)->refcnt;
}

void *
object_to_data (const struct object *obj)
{
  return obj->data[0];
}

struct object *
object_from_data (void *data)
{
  uintptr_t p = (uintptr_t)data;
    
  assert (p >= offsetof(struct object, data[0]));
  
  return (void *)(p - offsetof(struct object, data[0]));
}

unsigned long
object_num_cached (const struct object_ctx *ctx)
{
  return ctx->h->count;
}

void
object_iterate (const struct object_ctx *ctx,
                void (*func) (struct hash_backet *, void *),
                void *arg)
{
  hash_iterate (ctx->h, func, arg);
}

                      
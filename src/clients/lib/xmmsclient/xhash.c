/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "internal/xhash-int.h"
#include "internal/xutil-int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#define TRUE 1
#define FALSE 0

#define HASH_TABLE_MIN_SIZE 11
#define HASH_TABLE_MAX_SIZE 13845163

unsigned int
x_direct_hash (const void * v)
{
  return XPOINTER_TO_UINT (v);
}

int
x_direct_equal (const void * v1,
		const void * v2)
{
  return v1 == v2;
}

int
x_int_equal (const void * v1,
	     const void * v2)
{
  return *((const int*) v1) == *((const int*) v2);
}

unsigned int
x_int_hash (const void * v)
{
  return *(const int*) v;
}

int
x_str_equal (const void * v1,
	     const void * v2)
{
  const char *string1 = v1;
  const char *string2 = v2;
  
  return strcmp (string1, string2) == 0;
}

/* 31 bit hash function */
unsigned int
x_str_hash (const void *key)
{
  const char *p = key;
  unsigned int h = *p;

  if (h)
    for (p += 1; *p != '\0'; p++)
      h = (h << 5) - h + *p;

  return h;
}


typedef struct _x_hash_node_t      x_hash_node_t;

struct _x_hash_node_t
{
  void *   key;
  void *   value;
  x_hash_node_t *next;
};

struct _x_hash_t
{
  int             size;
  int             nnodes;
  x_hash_node_t      **nodes;
  XHashFunc        hash_func;
  XEqualFunc       key_equal_func;
};

#define X_HASH_TABLE_RESIZE(hash_table)				\
     if ((hash_table->size >= 3 * hash_table->nnodes &&	        \
	  hash_table->size > HASH_TABLE_MIN_SIZE) ||		\
	 (3 * hash_table->size <= hash_table->nnodes &&	        \
	  hash_table->size < HASH_TABLE_MAX_SIZE))		\
	   x_hash_resize (hash_table);			

static void		x_hash_resize	  (x_hash_t	  *hash_table);
static x_hash_node_t**	x_hash_lookup_node  (x_hash_t     *hash_table,
                                                   const void *   key);
static x_hash_node_t*	x_hash_node_new		  (void *	   key,
                                                   void *        value);
static void		x_hash_node_destroy	  (x_hash_node_t	  *hash_node);
static void		x_hash_nodes_destroy	  (x_hash_node_t	  *hash_node);


/**
 * x_hash_new:
 * @hash_func: a function to create a hash value from a key.
 *   Hash values are used to determine where keys are stored within the
 *   #x_hash_t data structure. The g_direct_hash(), g_int_hash() and 
 *   g_str_hash() functions are provided for some common types of keys. 
 *   If hash_func is %NULL, g_direct_hash() is used.
 * @key_equal_func: a function to check two keys for equality.  This is
 *   used when looking up keys in the #x_hash_t.  The g_direct_equal(),
 *   g_int_equal() and g_str_equal() functions are provided for the most
 *   common types of keys. If @key_equal_func is %NULL, keys are compared
 *   directly in a similar fashion to g_direct_equal(), but without the
 *   overhead of a function call.
 *
 * Creates a new #x_hash_t.
 * 
 * Return value: a new #x_hash_t.
 **/
x_hash_t*
x_hash_new (XHashFunc    hash_func,
		  XEqualFunc   key_equal_func)
{
  return x_hash_new_full (hash_func, key_equal_func);
}


/**
 * x_hash_new_full:
 * @hash_func: a function to create a hash value from a key.
 * @key_equal_func: a function to check two keys for equality.
 * @key_destroy_func: a function to free the memory allocated for the key 
 *   used when removing the entry from the #x_hash_t or %NULL if you 
 *   don't want to supply such a function.
 * @value_destroy_func: a function to free the memory allocated for the 
 *   value used when removing the entry from the #x_hash_t or %NULL if 
 *   you don't want to supply such a function.
 * 
 * Creates a new #x_hash_t like x_hash_new() and allows to specify
 * functions to free the memory allocated for the key and value that get 
 * called when removing the entry from the #x_hash_t.
 * 
 * Return value: a new #x_hash_t.
 **/
x_hash_t*
x_hash_new_full (XHashFunc       hash_func,
		       XEqualFunc      key_equal_func)
{
  x_hash_t *hash_table;
  unsigned int i;
  
  hash_table = malloc (sizeof (x_hash_t));
  hash_table->size               = HASH_TABLE_MIN_SIZE;
  hash_table->nnodes             = 0;
  hash_table->hash_func          = hash_func ? hash_func : x_direct_hash;
  hash_table->key_equal_func     = key_equal_func;
  hash_table->nodes              = malloc (sizeof (x_hash_node_t*) * hash_table->size);
  
  for (i = 0; i < hash_table->size; i++)
    hash_table->nodes[i] = NULL;
  
  return hash_table;
}

/**
 * x_hash_destroy:
 * @hash_table: a #x_hash_t.
 * 
 * Destroys the #x_hash_t. If keys and/or values are dynamically 
 * allocated, you should either free them first or create the #x_hash_t
 * using x_hash_new_full(). In the latter case the destroy functions 
 * you supplied will be called on all keys and values before destroying 
 * the #x_hash_t.
 **/
void
x_hash_destroy (x_hash_t *hash_table)
{
  unsigned int i;
  
  assert (hash_table != NULL);
  
  for (i = 0; i < hash_table->size; i++)
    x_hash_nodes_destroy (hash_table->nodes[i]);
  
  free (hash_table->nodes);
  free (hash_table);
}

static inline x_hash_node_t**
x_hash_lookup_node (x_hash_t	*hash_table,
			  const void *	 key)
{
  x_hash_node_t **node;
  
  node = &hash_table->nodes
    [(* hash_table->hash_func) (key) % hash_table->size];
  
  /* Hash table lookup needs to be fast.
   *  We therefore remove the extra conditional of testing
   *  whether to call the key_equal_func or not from
   *  the inner loop.
   */
  if (hash_table->key_equal_func)
    while (*node && !(*hash_table->key_equal_func) ((*node)->key, key))
      node = &(*node)->next;
  else
    while (*node && (*node)->key != key)
      node = &(*node)->next;
  
  return node;
}

/**
 * x_hash_lookup:
 * @hash_table: a #x_hash_t.
 * @key: the key to look up.
 * 
 * Looks up a key in a #x_hash_t.
 * 
 * Return value: the associated value, or %NULL if the key is not found.
 **/
void *
x_hash_lookup (x_hash_t	  *hash_table,
		     const void * key)
{
  x_hash_node_t *node;
  
  assert (hash_table != NULL);
  
  node = *x_hash_lookup_node (hash_table, key);
  
  return node ? node->value : NULL;
}

/**
 * x_hash_lookup_extended:
 * @hash_table: a #x_hash_t.
 * @lookup_key: the key to look up.
 * @orig_key: returns the original key.
 * @value: returns the value associated with the key.
 * 
 * Looks up a key in the #x_hash_t, returning the original key and the
 * associated value and a #int which is %TRUE if the key was found. This 
 * is useful if you need to free the memory allocated for the original key, 
 * for example before calling x_hash_remove().
 * 
 * Return value: %TRUE if the key was found in the #x_hash_t.
 **/
int
x_hash_lookup_extended (x_hash_t    *hash_table,
			      const void *  lookup_key,
			      void *	    *orig_key,
			      void *	    *value)
{
  x_hash_node_t *node;
  
  assert (hash_table != NULL);
  
  node = *x_hash_lookup_node (hash_table, lookup_key);
  
  if (node)
    {
      if (orig_key)
	*orig_key = node->key;
      if (value)
	*value = node->value;
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * x_hash_insert:
 * @hash_table: a #x_hash_t.
 * @key: a key to insert.
 * @value: the value to associate with the key.
 * 
 * Inserts a new key and value into a #x_hash_t.
 * 
 * If the key already exists in the #x_hash_t its current value is replaced
 * with the new value. If you supplied a @value_destroy_func when creating the 
 * #x_hash_t, the old value is freed using that function. If you supplied
 * a @key_destroy_func when creating the #x_hash_t, the passed key is freed 
 * using that function.
 **/
void
x_hash_insert (x_hash_t *hash_table,
		     void *	 key,
		     void *	 value)
{
  x_hash_node_t **node;
  
  assert (hash_table != NULL);
  
  node = x_hash_lookup_node (hash_table, key);
  
  if (*node)
    {
      /* do not reset node->key in this place, keeping
       * the old key is the intended behaviour. 
       * x_hash_replace() can be used instead.
       */

      (*node)->value = value;
    }
  else
    {
      *node = x_hash_node_new (key, value);
      hash_table->nnodes++;
      X_HASH_TABLE_RESIZE (hash_table);
    }
}

/**
 * x_hash_replace:
 * @hash_table: a #x_hash_t.
 * @key: a key to insert.
 * @value: the value to associate with the key.
 * 
 * Inserts a new key and value into a #x_hash_t similar to 
 * x_hash_insert(). The difference is that if the key already exists 
 * in the #x_hash_t, it gets replaced by the new key. If you supplied a 
 * @value_destroy_func when creating the #x_hash_t, the old value is freed 
 * using that function. If you supplied a @key_destroy_func when creating the 
 * #x_hash_t, the old key is freed using that function. 
 **/
void
x_hash_replace (x_hash_t *hash_table,
		      void *	  key,
		      void *	  value)
{
  x_hash_node_t **node;
  
  assert (hash_table != NULL);
  
  node = x_hash_lookup_node (hash_table, key);
  
  if (*node)
    {
      (*node)->key   = key;
      (*node)->value = value;
    }
  else
    {
      *node = x_hash_node_new (key, value);
      hash_table->nnodes++;
      X_HASH_TABLE_RESIZE (hash_table);
    }
}

/**
 * x_hash_remove:
 * @hash_table: a #x_hash_t.
 * @key: the key to remove.
 * 
 * Removes a key and its associated value from a #x_hash_t.
 *
 * If the #x_hash_t was created using x_hash_new_full(), the
 * key and value are freed using the supplied destroy functions, otherwise
 * you have to make sure that any dynamically allocated values are freed 
 * yourself.
 * 
 * Return value: %TRUE if the key was found and removed from the #x_hash_t.
 **/
int
x_hash_remove (x_hash_t	   *hash_table,
		     const void *  key)
{
  x_hash_node_t **node, *dest;
  
  assert (hash_table != NULL);
  
  node = x_hash_lookup_node (hash_table, key);
  if (*node)
    {
      dest = *node;
      (*node) = dest->next;
      x_hash_node_destroy (dest);
      hash_table->nnodes--;
  
      X_HASH_TABLE_RESIZE (hash_table);

      return TRUE;
    }

  return FALSE;
}

/**
 * x_hash_foreach_remove:
 * @hash_table: a #x_hash_t.
 * @func: the function to call for each key/value pair.
 * @user_data: user data to pass to the function.
 * 
 * Calls the given function for each key/value pair in the #x_hash_t.
 * If the function returns %TRUE, then the key/value pair is removed from the
 * #x_hash_t. If you supplied key or value destroy functions when creating
 * the #x_hash_t, they are used to free the memory allocated for the removed
 * keys and values.
 * 
 * Return value: the number of key/value pairs removed.
 **/
unsigned int
x_hash_foreach_remove (x_hash_t *hash_table,
                                      XHRFunc	  func,
                                      void *	  user_data)
{
  x_hash_node_t *node, *prev;
  unsigned int i;
  unsigned int deleted = 0;
  
  for (i = 0; i < hash_table->size; i++)
    {
    restart:
      
      prev = NULL;
      
      for (node = hash_table->nodes[i]; node; prev = node, node = node->next)
	{
	  if ((* func) (node->key, node->value, user_data))
	    {
	      deleted += 1;
	      
	      hash_table->nnodes -= 1;
	      
	      if (prev)
		{
		  prev->next = node->next;
		  x_hash_node_destroy (node);
		  node = prev;
		}
	      else
		{
		  hash_table->nodes[i] = node->next;
		  x_hash_node_destroy (node);
		  goto restart;
		}
	    }
	}
    }
  
  X_HASH_TABLE_RESIZE (hash_table);
  
  return deleted;
}

/**
 * x_hash_foreach:
 * @hash_table: a #x_hash_t.
 * @func: the function to call for each key/value pair.
 * @user_data: user data to pass to the function.
 * 
 * Calls the given function for each of the key/value pairs in the
 * #x_hash_t.  The function is passed the key and value of each
 * pair, and the given @user_data parameter.  The hash table may not
 * be modified while iterating over it (you can't add/remove
 * items). To remove all items matching a predicate, use
 * x_hash_remove().
 **/
void
x_hash_foreach (x_hash_t *hash_table,
		      XHFunc	  func,
		      void *	  user_data)
{
  x_hash_node_t *node;
  int i;
  
  assert (hash_table != NULL);
  assert (func != NULL);
  
  for (i = 0; i < hash_table->size; i++)
    for (node = hash_table->nodes[i]; node; node = node->next)
      (* func) (node->key, node->value, user_data);
}

/**
 * x_hash_size:
 * @hash_table: a #x_hash_t.
 * 
 * Returns the number of elements contained in the #x_hash_t.
 * 
 * Return value: the number of key/value pairs in the #x_hash_t.
 **/
unsigned int
x_hash_size (x_hash_t *hash_table)
{
  assert (hash_table != NULL);
  
  return hash_table->nnodes;
}


static const unsigned int x_primes[] =
{
  11,
  19,
  37,
  73,
  109,
  163,
  251,
  367,
  557,
  823,
  1237,
  1861,
  2777,
  4177,
  6247,
  9371,
  14057,
  21089,
  31627,
  47431,
  71143,
  106721,
  160073,
  240101,
  360163,
  540217,
  810343,
  1215497,
  1823231,
  2734867,
  4102283,
  6153409,
  9230113,
  13845163,
};

static const unsigned int x_nprimes = sizeof (x_primes) / sizeof (x_primes[0]);

static unsigned int
x_spaced_primes_closest (unsigned int num)
{
  int i;

  for (i = 0; i < x_nprimes; i++)
    if (x_primes[i] > num)
      return x_primes[i];

  return x_primes[x_nprimes - 1];
}

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

static void
x_hash_resize (x_hash_t *hash_table)
{
  x_hash_node_t **new_nodes;
  x_hash_node_t *node;
  x_hash_node_t *next;
  unsigned int hash_val;
  int new_size;
  int i;

  new_size = x_spaced_primes_closest (hash_table->nnodes);
  new_size = CLAMP (new_size, HASH_TABLE_MIN_SIZE, HASH_TABLE_MAX_SIZE);
 
  new_nodes = malloc (sizeof (x_hash_node_t*) * new_size);
  
  for (i = 0; i < hash_table->size; i++)
    for (node = hash_table->nodes[i]; node; node = next)
      {
	next = node->next;

	hash_val = (* hash_table->hash_func) (node->key) % new_size;

	node->next = new_nodes[hash_val];
	new_nodes[hash_val] = node;
      }
  
  free (hash_table->nodes);
  hash_table->nodes = new_nodes;
  hash_table->size = new_size;
}

static x_hash_node_t*
x_hash_node_new (void * key,
		 void * value)
{
  x_hash_node_t *hash_node;
  
  hash_node = malloc (sizeof (x_hash_node_t));
  
  hash_node->key = key;
  hash_node->value = value;
  hash_node->next = NULL;
  
  return hash_node;
}

static void
x_hash_node_destroy (x_hash_node_t      *hash_node)
{
  free (hash_node);
}

static void
x_hash_nodes_destroy (x_hash_node_t *hash_node)
{
  while (hash_node)
    {
      x_hash_node_t *next = hash_node->next;

      free (hash_node);
      hash_node = next;
    }  
}

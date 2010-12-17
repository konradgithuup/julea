/*
 * Copyright (c) 2010 Michael Kuhn
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * \file
 */

#include <glib.h>

#include <bson.h>
#include <mongo.h>

#include "jcollection.h"
#include "jcollection-internal.h"

#include "jbson.h"
#include "jconnection.h"
#include "jconnection-internal.h"
#include "jitem.h"
#include "jitem-internal.h"
#include "jsemantics.h"
#include "jstore.h"

/**
 * \defgroup JCollection Collection
 * @{
 */

struct JCollection
{
	gchar* name;

	struct
	{
		gchar* items;
	}
	collection;

	JSemantics* semantics;
	JStore* store;

	guint ref_count;
};

JCollection*
j_collection_new (const gchar* name)
{
	JCollection* collection;

	g_return_val_if_fail(name != NULL, NULL);

	/*
	: m_initialized(false),

	m_id.init();
	*/

	collection = g_new(JCollection, 1);
	collection->name = g_strdup(name);
	collection->collection.items = NULL;
	collection->semantics = NULL;
	collection->store = NULL;
	collection->ref_count = 1;

	return collection;
}

JCollection*
j_collection_ref (JCollection* collection)
{
	g_return_val_if_fail(collection != NULL, NULL);

	collection->ref_count++;

	return collection;
}

void
j_collection_unref (JCollection* collection)
{
	g_return_if_fail(collection != NULL);

	collection->ref_count--;

	if (collection->ref_count == 0)
	{
		if (collection->semantics != NULL)
		{
			j_semantics_unref(collection->semantics);
		}

		if (collection->store != NULL)
		{
			j_store_unref(collection->store);
		}

		g_free(collection->collection.items);
		g_free(collection->name);
		g_free(collection);
	}
}

const gchar*
j_collection_name (JCollection* collection)
{
	g_return_val_if_fail(collection != NULL, NULL);

	return collection->name;
}

void
j_collection_create (JCollection* collection, GQueue* items)
{
	JBSON* index;
	JBSON** jobj;
	JSemantics* semantics;
	mongo_connection* mc;
	bson** obj;
	guint length;
	guint i;

	g_return_if_fail(collection != NULL);
	g_return_if_fail(items != NULL);

	/*
	IsInitialized(true);
	*/

	length = g_queue_get_length(items);

	if (length == 0)
	{
		return;
	}

	jobj = g_new(JBSON*, length);
	obj = g_new(bson*, length);
	i = 0;

	for (GList* l = items->head; l != NULL; l = l->next)
	{
		JItem* item = l->data;
		JBSON* jbson;

		j_item_associate(item, collection);
		jbson = j_item_serialize(item);

		jobj[i] = jbson;
		obj[i] = j_bson_get(jbson);

		i++;
	}

	mc = j_connection_connection(j_store_connection(collection->store));

	index = j_bson_new();
	j_bson_append_int(index, "Collection", 1);
	j_bson_append_int(index, "Name", 1);

	mongo_create_index(mc, j_collection_collection_items(collection), j_bson_get(index), MONGO_INDEX_UNIQUE, NULL);

	j_bson_free(index);

	mongo_insert_batch(mc, j_collection_collection_items(collection), obj, length);

	for (i = 0; i < length; i++)
	{
		j_bson_free(jobj[i]);
	}

	g_free(jobj);
	g_free(obj);

	semantics = j_collection_semantics(collection);

	if (j_semantics_get(semantics, J_SEMANTICS_PERSISTENCY) == J_SEMANTICS_PERSISTENCY_STRICT)
	{
		bson ores;

		mongo_simple_int_command(mc, "admin", "fsync", 1, &ores);
		//bson_print(&ores);
		bson_destroy(&ores);
	}
}

GQueue*
j_collection_get (JCollection* collection, GQueue* names)
{
	JBSON* empty;
	JBSON* jbson;
	mongo_connection* mc;
	mongo_cursor* cursor;
	GQueue* items;
	guint length;
	guint n = 0;

	g_return_val_if_fail(collection != NULL, NULL);
	g_return_val_if_fail(names != NULL, NULL);

	/*
		IsInitialized(true);
	*/

	jbson = j_bson_new();
	length = g_queue_get_length(names);

	/* FIXME */
	j_bson_append_str(jbson, "Collection", collection->name);

	if (length == 1)
	{
		const gchar* name = names->head->data;

		j_bson_append_str(jbson, "Name", name);
		n = 1;
	}
	else
	{
		j_bson_append_object_start(jbson, "$or");

		for (GList* l = names->head; l != NULL; l = l->next)
		{
			const gchar* name = l->data;

			j_bson_append_str(jbson, "Name", name);
		}

		j_bson_append_object_end(jbson);
	}

	empty = j_bson_new_empty();

	mc = j_connection_connection(j_store_connection(collection->store));
	cursor = mongo_find(mc, j_collection_collection_items(collection), j_bson_get(jbson), j_bson_get(empty), n, 0, 0);

	items = g_queue_new();

	while (mongo_cursor_next(cursor))
	{
		JBSON* item_bson;

		item_bson = j_bson_new_from_bson(&(cursor->current));
		g_queue_push_tail(items, j_item_new_from_bson(collection, item_bson));
		j_bson_free(item_bson);
	}

	mongo_cursor_destroy(cursor);

	j_bson_free(empty);
	j_bson_free(jbson);

	return items;
}

JSemantics*
j_collection_semantics (JCollection* collection)
{
	g_return_val_if_fail(collection != NULL, NULL);

	if (collection->semantics != NULL)
	{
		return collection->semantics;
	}

	return j_store_semantics(collection->store);
}

void
j_collection_set_semantics (JCollection* collection, JSemantics* semantics)
{
	g_return_if_fail(collection != NULL);
	g_return_if_fail(semantics != NULL);

	if (collection->semantics != NULL)
	{
		j_semantics_unref(collection->semantics);
	}

	collection->semantics = j_semantics_ref(semantics);
}

/* Internal */

JCollection*
j_collection_new_from_bson (JStore* store, JBSON* jbson)
{
	/*
		: m_initialized(true),
	*/
	JCollection* collection;

	g_return_val_if_fail(store != NULL, NULL);
	g_return_val_if_fail(jbson != NULL, NULL);

	collection = g_new(JCollection, 1);
	collection->name = NULL;
	collection->collection.items = NULL;
	collection->semantics = NULL;
	collection->store = j_store_ref(store);
	collection->ref_count = 1;

	j_collection_deserialize(collection, jbson);

	return collection;
}

const gchar*
j_collection_collection_items (JCollection* collection)
{
	g_return_val_if_fail(collection != NULL, NULL);
	g_return_val_if_fail(collection->store != NULL, NULL);

	if (collection->collection.items == NULL)
	{
		collection->collection.items = g_strdup_printf("%s.Items", j_store_name(collection->store));
	}

	return collection->collection.items;
}

JStore*
j_collection_store (JCollection* collection)
{
	g_return_val_if_fail(collection != NULL, NULL);

	return collection->store;
}

void
j_collection_associate (JCollection* collection, JStore* store)
{
	g_return_if_fail(collection != NULL);
	g_return_if_fail(store != NULL);

	/*
		IsInitialized(false);

		m_initialized = true;
	*/

	collection->store = j_store_ref(store);
}

JBSON*
j_collection_serialize (JCollection* collection)
{
	/*
			.append("User", m_owner.User())
			.append("Group", m_owner.Group())
	*/

	JBSON* jbson;

	g_return_val_if_fail(collection != NULL, NULL);

	jbson = j_bson_new();
	j_bson_append_new_id(jbson, "_id");
	j_bson_append_str(jbson, "Name", collection->name);

	return jbson;
}

void
j_collection_deserialize (JCollection* collection, JBSON* jbson)
{
	g_return_if_fail(collection != NULL);
	g_return_if_fail(jbson != NULL);

	/*
		m_id = o.getField("_id").OID();
		m_name = o.getField("Name").String();

		m_owner.m_user = o.getField("User").Int();
		m_owner.m_group = o.getField("Group").Int();
	*/
}

/*
namespace JULEA
{
	void _Collection::IsInitialized (bool check) const
	{
		if (m_initialized != check)
		{
			if (check)
			{
				throw Exception(JULEA_FILELINE ": Collection not initialized.");
			}
			else
			{
				throw Exception(JULEA_FILELINE ": Collection already initialized.");
			}
		}
	}

	mongo::OID const& _Collection::ID () const
	{
		return m_id;
	}
}
*/

/**
 * @}
 */
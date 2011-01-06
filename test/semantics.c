/*
 * Copyright (c) 2010-2011 Michael Kuhn
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

#include <glib.h>

#include <julea.h>

static void
test_semantics_fixture_setup (JSemantics** semantics, gconstpointer data)
{
	*semantics = j_semantics_new();
}

static void
test_semantics_fixture_teardown (JSemantics** semantics, gconstpointer data)
{
	j_semantics_unref(*semantics);
}

static void
test_semantics_new_ref_unref (gpointer* fixture, gconstpointer data)
{
	const guint n = 100000;

	for (guint i = 0; i < n; i++)
	{
		JSemantics* semantics;

		semantics = j_semantics_new();
		g_assert(semantics != NULL);
		semantics = j_semantics_ref(semantics);
		g_assert(semantics != NULL);
		j_semantics_unref(semantics);
		j_semantics_unref(semantics);
	}
}

static void
test_semantics_set_get (JSemantics** semantics, gconstpointer data)
{
	gint s;

	j_semantics_set(*semantics, J_SEMANTICS_CONCURRENCY, J_SEMANTICS_CONCURRENCY_STRICT);
	s = j_semantics_get(*semantics, J_SEMANTICS_CONCURRENCY);
	g_assert_cmpint(s, ==, J_SEMANTICS_CONCURRENCY_STRICT);
	j_semantics_set(*semantics, J_SEMANTICS_CONSISTENCY, J_SEMANTICS_CONSISTENCY_STRICT);
	s = j_semantics_get(*semantics, J_SEMANTICS_CONSISTENCY);
	g_assert_cmpint(s, ==, J_SEMANTICS_CONSISTENCY_STRICT);
	j_semantics_set(*semantics, J_SEMANTICS_PERSISTENCY, J_SEMANTICS_PERSISTENCY_LAX);
	s = j_semantics_get(*semantics, J_SEMANTICS_PERSISTENCY);
	g_assert_cmpint(s, ==, J_SEMANTICS_PERSISTENCY_LAX);
	j_semantics_set(*semantics, J_SEMANTICS_SECURITY, J_SEMANTICS_SECURITY_STRICT);
	s = j_semantics_get(*semantics, J_SEMANTICS_SECURITY);
	g_assert_cmpint(s, ==, J_SEMANTICS_SECURITY_STRICT);
}

int main (int argc, char** argv)
{
	g_test_init(&argc, &argv, NULL);

	g_test_add("/julea/semantics/new_ref_unref", gpointer, NULL, NULL, test_semantics_new_ref_unref, NULL);

	g_test_add("/julea/semantics/set_get", JSemantics*, NULL, test_semantics_fixture_setup, test_semantics_set_get, test_semantics_fixture_teardown);

	return g_test_run();
}
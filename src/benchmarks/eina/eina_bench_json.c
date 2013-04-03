/* EINA - EFL data type library
 * Copyright (C) 2013 Yossi Kantor
                      Cedric Bail
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library;
 * if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifdef EINA_BENCH_HAVE_GLIB
# include <glib.h>
#endif

#include "Evas_Data.h"
#include "Ecore_Data.h"

#include "eina_json.h"
#include "eina_main.h"
#include "eina_bench.h"

static void
eina_bench_json_parse(int request)
{
   int i;
   eina_init();

   Eina_Json_Context *jctx = eina_json_context_dom_new();
   eina_json_context_parse(jctx, "{");
   for (i = 0; i < request; i++)
     eina_json_context_parse(jctx, "\"Bench\":[\"Json\", 34.5, true, null],");
   eina_json_context_parse(jctx, "\"End\":null }");

   eina_shutdown();
}

void eina_bench_json(Eina_Benchmark *bench)
{
   eina_benchmark_register(bench, "json",
                           EINA_BENCHMARK(eina_bench_json_parse),
                           100, 100000, 500);
}

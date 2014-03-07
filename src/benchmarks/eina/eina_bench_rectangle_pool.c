/* EINA - EFL data type library
 * Copyright (C) 2008 Cedric Bail
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

#include "eina_bench.h"
#include "Eina.h"

#define W 1024
#define H 512
#define RW 64
#define RH 64

static void
eina_bench_generic_packing_efficiency(int request, Eina_Rectangle_Packing type)
{
   Eina_Rectangle_Pool *pool;
   Eina_Rectangle *rect;
   Eina_List *list = NULL, *list1 = NULL;
   int current_descending=0;
   int count = 0;
   int i;

   pool = eina_rectangle_pool_new(W, H);
   eina_rectangle_pool_packing_set(pool, type);

   if (!pool)
      return;

   for (i = 0; i < request; ++i)
     {
        rect = NULL;

        while (!rect)
          {
             rect = eina_rectangle_pool_request(pool, rand() % RW + 1, rand() % RH + 1);
             if (!rect)
               {
                  list1 = eina_list_last(list);
                  rect = eina_list_data_get(list1);
                  list = eina_list_remove_list(list, list1);
                  if (rect)
                    {
                       count = 1;
                       eina_rectangle_pool_release(rect);
                    }
               }
            else
              {
                 if (!count)
                   current_descending++;
                 list = eina_list_append(list, rect);
              }
          }
     }

   eina_rectangle_pool_free(pool);
   eina_list_free(list);
}

#define EINA_BENCH_PACKING(Name)                                        \
  static void                                                           \
  eina_bench_packing_#Name(int request)                                 \
  {                                                                     \
     eina_bench_generic_packing_efficiency(request, Eina_Packing_#Name); \
  }

EINA_BENCH_PACKING(Descending);
EINA_BENCH_PACKING(Ascending);
EINA_BENCH_PACKING(Bottom_Left);
EINA_BENCH_PACKING(Bottom_Left_Skyline);

void eina_bench_rectangle_pool(Eina_Benchmark *bench)
{
#define EINA_BENCH_REGISTER(Name)                                       \
   eina_benchmark_register(bench, "eina - "##Name,                      \
                           EINA_BENCHMARK(eina_bench_packing_#Name),    \
                           10, 4000, 100);

   EINA_BENCH_REGISTER(Descending);
   EINA_BENCH_REGISTER(Ascending);
   EINA_BENCH_REGISTER(Bottom_Left);
   EINA_BENCH_REGISTER(Bottom_Left_Skyline);
}



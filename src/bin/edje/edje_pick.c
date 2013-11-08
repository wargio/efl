#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Edje_Pick.h"

int
main(int argc, char **argv)
{
   Edje_Pick *context = edje_pick_context_new();
   edje_pick_context_set(context);
   int status;

   ecore_app_no_system_modules();

   edje_pick_init();
   eina_log_level_set(EINA_LOG_LEVEL_WARN);  /* Changed to INFO if verbose */

   status = edje_pick_process(argc, argv);

   edje_pick_context_free(context);
   edje_pick_shutdown();
   return status;
}

#include "Edje_Pick.h"
#include <Ecore_Getopt.h>
#include <Ecore.h>

#define EDJE_PICK_HELP_STRING \
"\nEdje Pick - the \"edj\" merging tool.\n\
===================================\n\n\
Use Edje Pick to compose a single edj file \
by selecting groups from edj files.\n\n\
Use Edje Pick as follows:\n\
Include files with \'--include\' file-name\n\
Follow the included file-name by names of selected groups from this file.\n\
To select a group use: \'--group\' group-name.\n\n\
You must specify merged file name with \'--output\' file-name.\n\
Use '--verbose' switch to get detailed log.\n\n\
To produce 3rd file \'out.edj\' that composed  of:\n\
Group \'test\' from \'ex1.edj\' and \'test2\', \'test3\', from \'ex2.edj\'\n\
edje_pick -i ex1.edj -g test -i ex2.edj -g test2 -g test3 -o out.edj\n\n\
By using \'--append\' whole file content is selected.\n\
No need to specify selected groups with this switch.\n\
Note that selected group with will override group with the same name\n\
from appended-file when using \'--append\' switch.\n\n\
Example, the command:\n\
edje_pick -a theme1.edj -i default.edj -g elm/button/base/default \
-g elm/check/base/default -o hybrid.edj\n\n\
will produce a 3rd theme file \'hybrid.edj\',\n\
composed of all theme1.edj components.\n\
Replacing the button and check with widgets taken from default theme.\n\
(Given that theme1.edj button, check group-name are as in default.edj)\n"

static Edje_Pick_Status
_args_parse_into_session(int argc, char **argv, Edje_Pick_Session *session)
{  /* On return ifs is Input Files List, ofn is Output File Name */
   Eina_List *gpf = NULL; /* List including counters of groups-per-file */
   Eina_List *a_files = NULL;
   Eina_List *i_files = NULL;
   Eina_List *groups = NULL;
   Eina_List *itr, *itr2, *cg;
   Eina_Bool verbose = EINA_FALSE;
   char *output_filename = NULL;
   int *c = NULL;
   int k;
   const char *str;
   Eina_Bool show_help = EINA_FALSE;
   Edje_Pick_Status status = EDJE_PICK_NO_ERROR;

   /* Define args syntax */
#define IS_GROUP(x) ((!strcmp(x, "-g")) || (!strcmp(x, "--group")))
#define IS_INCLUDE(x) ((!strcmp(x, "-i")) || (!strcmp(x, "--include")))
#define IS_HELP(x) ((!strcmp(x, "-h")) || (!strcmp(x, "--help")))
   static const Ecore_Getopt optdesc = {
        "edje_pick",
        NULL,
        "0.0",
        "(C) 2012 Enlightenment",
        "Public domain?",
        "Edje Pick - the \"edj\" merging tool.",

        EINA_TRUE,
        {
           ECORE_GETOPT_STORE_TRUE('v', "verbose", "Verbose"),
           ECORE_GETOPT_STORE('o', "output", "Output File",
                 ECORE_GETOPT_TYPE_STR),
           ECORE_GETOPT_APPEND_METAVAR('a', "append", "Append File",
                 "STRING", ECORE_GETOPT_TYPE_STR),
           ECORE_GETOPT_APPEND_METAVAR('i', "include", "Include File",
                 "STRING", ECORE_GETOPT_TYPE_STR),
           ECORE_GETOPT_APPEND_METAVAR('g', "group", "Add Group",
                 "STRING", ECORE_GETOPT_TYPE_STR),
           ECORE_GETOPT_HELP('h', "help"),
           ECORE_GETOPT_SENTINEL
        }
   };

   Ecore_Getopt_Value values[] = {
        ECORE_GETOPT_VALUE_BOOL(verbose),
        ECORE_GETOPT_VALUE_STR(output_filename),
        ECORE_GETOPT_VALUE_LIST(a_files),
        ECORE_GETOPT_VALUE_LIST(i_files),
        ECORE_GETOPT_VALUE_LIST(groups),
        ECORE_GETOPT_VALUE_NONE
   };

   /* START - Read command line args */
   c = NULL;
   for(k = 1; k < argc; k++)
     {  /* Run through args, count how many groups per file */
        if(IS_GROUP(argv[k]))
          {
             if (!c)
               {
                  status = EDJE_PICK_INCLUDE_MISSING;
                  goto end;
               }

             (*c)++;
             continue;
          }

        if(IS_INCLUDE(argv[k]))
          {
             c = calloc(1, sizeof(int));
             gpf = eina_list_append(gpf, c);
             continue;
          }

        show_help |= IS_HELP(argv[k]);
     }


   if (ecore_getopt_parse(&optdesc, values, argc, argv) < 0)
     {
        status = EDJE_PICK_PARSE_FAILED;
        goto end;
     }

   if (show_help)
     {
        printf(EDJE_PICK_HELP_STRING);
        goto end;
     }

   if (verbose)  /* Changed to INFO if verbose */
     {
        eina_log_level_set(EINA_LOG_LEVEL_INFO);
     }
   edje_pick_session_verbose_set(session, verbose);

   EINA_LIST_FOREACH(a_files, itr, str)
      edje_pick_input_file_add(session, str);

   itr2 = gpf;
   cg = groups;
   EINA_LIST_FOREACH(i_files, itr, str)
     {  /* Now match groups from groups-list with included files */
        c = eina_list_data_get(itr2);
        if (c)
          {
             while(*c)
               {
                  if (!edje_pick_group_add(session, str, eina_list_data_get(cg)))
                    {
                       status = EDJE_PICK_FAILED_ADD_GROUP;
                       goto end;
                    }
                  cg = eina_list_next(cg);
                  (*c)--;
               }
          }
        itr2 = eina_list_next(itr2);
     }

   edje_pick_output_file_add(session, output_filename);
end:
   EINA_LIST_FREE(gpf, c)
      free(c);

   ecore_getopt_list_free(a_files);
   a_files = NULL;
   ecore_getopt_list_free(i_files);
   i_files = NULL;
   ecore_getopt_list_free(groups);
   groups = NULL;

   if (!output_filename) return EDJE_PICK_OUT_FILENAME_MISSING;
   return status;
}

int
main(int argc, char **argv)
{
   ecore_app_no_system_modules();

   edje_pick_init();
   Edje_Pick_Session *session = edje_pick_session_new();

   eina_log_level_set(EINA_LOG_LEVEL_WARN);  /* Changed to INFO if verbose */

   _args_parse_into_session(argc, argv, session);
   int status = edje_pick_process(session);

   edje_pick_session_del(session);
   edje_pick_shutdown();
   return status;
}

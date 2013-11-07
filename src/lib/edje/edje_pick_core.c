#include "Edje_Pick.h"
#include "edje_private.h"

static Edje_Pick *context = NULL;
static unsigned char *init_count = 0;

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

#define VERBOSE(COMMAND) if (context->v) { COMMAND; }

struct _Edje_Pick_Id
{
   int old_id;
   int new_id;
   Eina_Bool used;
};
typedef struct _Edje_Pick_Id Edje_Pick_Id;

struct _Edje_Pick_File_Params
{
   const char *name;
   Eina_List *groups;
   Edje_File *edf;     /* Keeps all file data after reading  */
   Eina_Bool append;   /* Take everything from this file */

   /* We hold list of IDs for each file */
   Eina_List *scriptlist;
   Eina_List *luascriptlist;
   Eina_List *imagelist;
   Eina_List *imagesetlist;  /* List of IDs (Edje_Pick_Data) for image sets */
   Eina_List *samplelist;
   Eina_List *tonelist;
};
typedef struct _Edje_Pick_File_Params Edje_Pick_File_Params;

struct _Edje_Pick_Data
{
   const char *filename;  /* Image, Sample File Name       */
   void *entry ;          /* used to build output file dir FIXME: REMOVE THIS */
   void *data;            /* Data as taken from input file */

   int size;
   Edje_Pick_Id id;
};
typedef struct _Edje_Pick_Data Edje_Pick_Data;

struct _Edje_Pick_Tone
{
   Edje_Sound_Tone *tone;
   Eina_Bool used;
};
typedef struct _Edje_Pick_Tone Edje_Pick_Tone;

struct _Edje_Pick_Font
{
   Edje_Font *f;
   Eina_Bool used;
};
typedef struct _Edje_Pick_Font Edje_Pick_Font;

struct _Edje_Pick
{
   Eina_Bool v; /* Verbose */
   int current_group_id;
   Edje_Pick_File_Params *current_file;
   Eina_List *fontlist;
   const char *errfile;
   const char *errgroup;
};

EAPI void
edje_pick_init(void)
{
   if (!init_count)
     {
        eina_init();
        eet_init();
        ecore_init();
        _edje_edd_init();
     }

   init_count++;
}

EAPI void
edje_pick_shutdown(void)
{

   if (init_count)
     {
        init_count--;

        if (!init_count)
          {  /* When count reach 0, do shutdown */
             _edje_edd_shutdown();
             eet_shutdown();
             ecore_shutdown();
             eina_shutdown();
          }
     }
}

EAPI Edje_Pick *
edje_pick_context_new(void)
{
   return calloc(1, sizeof(Edje_Pick));
}

EAPI void
edje_pick_context_free(Edje_Pick *c)
{
   free(c);
}

EAPI void
edje_pick_context_set(Edje_Pick *c)
{
   context = c;
}

static void
_edje_pick_data_free(Eina_List *l)
{
   Edje_Pick_Data *ep;

   EINA_LIST_FREE(l, ep)
     {
        if (ep->filename) eina_stringshare_del(ep->filename);
        free(ep->data);
        free(ep);
     }
}

static void
_edje_pick_out_file_free(Edje_File *out_file)
{
   if (out_file)
     {
        /* Free output file memory allocation */
        if (out_file->ef)
          eet_close(out_file->ef);

        if (out_file->external_dir)
          {
             if (out_file->external_dir->entries)
               free(out_file->external_dir->entries);

             free(out_file->external_dir);
          }

        if (out_file->image_dir)
          {
             if (out_file->image_dir->entries)
               free(out_file->image_dir->entries);

             free(out_file->image_dir);
          }

        if (out_file->sound_dir)
          {
             if (out_file->sound_dir->samples)
               free(out_file->sound_dir->samples);

             if (out_file->sound_dir->tones)
               free(out_file->sound_dir->tones);

             free(out_file->sound_dir);
          }

        eina_list_free(out_file->color_classes);
        eina_hash_free_cb_set(out_file->collection, free);
        eina_hash_free(out_file->collection);
        eina_stringshare_del(out_file->compiler);

        free(out_file);
     }
}

static void
_edje_pick_args_show(Eina_List *ifs, char *out)
{  /* Print command-line arguments after parsing phase */
   Edje_Pick_File_Params *p;
   Eina_List *l;
   char *g;

   EINA_LOG_INFO("Got args for <%d> input files.\n", eina_list_count(ifs));

   EINA_LIST_FOREACH(ifs, l, p)
     {
        Eina_List *ll;

        if (p->append)
          printf("\nFile name: %s\n\tGroups: ALL (append mode)\n", p->name);
        else
          {
             printf("\nFile name: %s\n\tGroups:\n", p->name);
             EINA_LIST_FOREACH(p->groups, ll, g)
                printf("\t\t%s\n", g);
          }
     }

   EINA_LOG_INFO("\nOutput file name was <%s>\n", out);
}

EAPI const char *
edje_pick_err_str_get(Edje_Pick_Status s)
{
   const char *err = NULL;

   switch (s)
     {
      case EDJE_PICK_NO_ERROR:
         err = NULL;
         break;
      case EDJE_PICK_OUT_FILENAME_MISSING:
         err = "Output file name missing.";
         break;
      case EDJE_PICK_FAILED_OPEN_INP:
         err = "Failed to open input file.";
         break;
      case EDJE_PICK_FAILED_READ_INP:
         err = "Failed to read input file.";
         break;
      case EDJE_PICK_DUP_GROUP:
         err = "Can't fetch groups with identical name from various files.";
         break;
      case EDJE_PICK_NO_GROUP:
         err = "No groups to fetch from input files.";
         break;
      case EDJE_PICK_INCLUDE_MISSING:
         err = "Cannot select groups when no input file included.";
         break;
      case EDJE_PICK_GROUP_MISSING:
         err = "Group name missing for include file.";
         break;
      case EDJE_PICK_PARSE_FAILED:
         err = "Command parsing failed.";
         break;
      case EDJE_PICK_FILE_OPEN_FAILED:
         err = "Failed to open file.";
         break;
      case EDJE_PICK_FILE_READ_FAILED:
         err = "Failed to read file.";
         break;
      case EDJE_PICK_FILE_COLLECTION_EMPTY:
         err = "File collection is empty (corrupted?).";
         break;
      default:
         err = "Undefined Status";
     }

   return err;
}

static int
_edje_pick_cleanup(Eina_List *ifs, Edje_File *out_file, Edje_Pick_Status s,
      Eina_List *a_files, Eina_List *i_files, Eina_List *groups,
      const char *errfile, const char *errgroup)
{
   Edje_Pick_File_Params *p;
   Edje_Pick_Font *ft;
   void *n;

   if (context)
     {  /* Update User struct with err data */
        if (context->errfile) eina_stringshare_del(context->errfile);
        if (context->errgroup) eina_stringshare_del(context->errgroup);

        context->errfile = context->errgroup = NULL;
        if (errfile) context->errfile = eina_stringshare_add(errfile);
        if (errgroup) context->errgroup = eina_stringshare_add(errgroup);
     }

   if (a_files) ecore_getopt_list_free(a_files);
   if (i_files) ecore_getopt_list_free(i_files);
   if (groups) ecore_getopt_list_free(groups);
   _edje_pick_out_file_free(out_file);

   EINA_LIST_FREE(ifs, p)
     {
        EINA_LIST_FREE(p->groups, n)
          eina_stringshare_del(n);

        _edje_pick_data_free(p->scriptlist);
        p->scriptlist = NULL;

        _edje_pick_data_free(p->luascriptlist);
        p->luascriptlist = NULL;

        _edje_pick_data_free(p->imagelist);
        p->imagelist = NULL;

        _edje_pick_data_free(p->imagesetlist);
        p->imagesetlist = NULL;

        _edje_pick_data_free(p->samplelist);

        EINA_LIST_FREE(p->tonelist, n)
          free(n);

        if (p->edf)
          _edje_cache_file_unref(p->edf);

        free(p);
     }

   EINA_LIST_FREE(context->fontlist, ft)
     {
        Edje_Font *st = ft->f;

        eina_stringshare_del(st->name);
        eina_stringshare_del(st->file);
        free(st);
        free(ft);
     }

   if (edje_pick_err_str_get(s))
     EINA_LOG_ERR("%s\n", edje_pick_err_str_get(s));

   context = NULL;

   return s;
}

/* Look for group name in all input files that are not d1 */
static int
_group_name_in_other_file(Eina_List *inp_files, void *d1, void *d2)
{
   Edje_Pick_File_Params *inp_file = d1;
   char *group = d2; /* Group name to search */
   Eina_List *f;
   Edje_Pick_File_Params *current_file;

   EINA_LIST_FOREACH(inp_files, f, current_file)
     {
        if (inp_file->name != current_file->name)
          if (eina_list_search_unsorted(current_file->groups,
                   (Eina_Compare_Cb) strcmp,
                   group))
            return 1;
     }

   return 0;  /* Not found */
}

EAPI int
edje_pick_command_line_parse(int argc, char **argv,
			      Eina_List **ifs, char **ofn,
                              Eina_Bool cleanup)
{  /* On return ifs is Input Files List, ofn is Output File Name */
   Eina_List *gpf = NULL; /* List including counters of groups-per-file */
   Eina_List *a_files = NULL;
   Eina_List *i_files = NULL;
   Eina_List *l;
   Eina_List *ll;
   Eina_List *cg;
   Eina_List *groups = NULL;
   char *output_filename = NULL;
   Edje_Pick_File_Params *current_inp = NULL;
   Eina_List *files = NULL;  /* List of input files */
   int *c = NULL;
   char *str = NULL;
   int k;
   Eina_Bool show_help = EINA_FALSE;

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
        ECORE_GETOPT_VALUE_BOOL(context->v),
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
               return _edje_pick_cleanup(files, NULL,
                     EDJE_PICK_INCLUDE_MISSING, a_files, i_files, groups,
                     NULL, NULL);

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

   if (show_help)
     puts(EDJE_PICK_HELP_STRING);

   if (ecore_getopt_parse(&optdesc, values, argc, argv) < 0)
     {
        EINA_LIST_FREE(gpf, c)
           free(c);

        return _edje_pick_cleanup(files, NULL, EDJE_PICK_PARSE_FAILED,
              a_files, i_files, groups, NULL, NULL);
     }

   if (show_help)
     {
        EINA_LIST_FREE(gpf, c)
           free(c);

        return _edje_pick_cleanup(files, NULL, EDJE_PICK_HELP_SHOWN,
              a_files, i_files, groups, NULL, NULL);
     }

   if (context->v)  /* Changed to INFO if verbose */
     eina_log_level_set(EINA_LOG_LEVEL_INFO);

   EINA_LIST_FOREACH(a_files, l, str)
     {
        current_inp = calloc(1, sizeof(*current_inp));
        current_inp->append = EINA_TRUE;
        current_inp->name = eina_stringshare_add(str);
        files = eina_list_append(files, current_inp);
     }

   ll = gpf;
   cg = groups;
   EINA_LIST_FOREACH(i_files, l, str)
     {  /* Now match groups from groups-list with included files */
        current_inp = calloc(1, sizeof(*current_inp));
        current_inp->name = eina_stringshare_add(str);
        files = eina_list_append(files, current_inp);
        c = eina_list_data_get(ll);
        if (c)
          {
             while(*c)
               {
                  char *g_name;
                  if (!cg)
                    {
                       EINA_LIST_FREE(gpf, c)
                          free(c);

                       return _edje_pick_cleanup(files, NULL,
                             EDJE_PICK_GROUP_MISSING,
                             a_files, i_files, groups, NULL, NULL);
                    }


                  g_name = eina_list_data_get(cg);
                  if (_group_name_in_other_file(files, current_inp, g_name))
                    return _edje_pick_cleanup(files, NULL, EDJE_PICK_DUP_GROUP,
                          a_files, i_files, groups, current_inp->name, g_name);

                  if (!eina_list_search_unsorted(current_inp->groups,
                           (Eina_Compare_Cb) strcmp, g_name))
                    {
                       current_inp->groups = eina_list_append(
                             current_inp->groups, eina_stringshare_add(g_name));
                    }

                  cg = eina_list_next(cg);
                  (*c)--;
               }
          }
        ll = eina_list_next(ll);
     }

   EINA_LIST_FREE(gpf, c)
      free(c);

   ecore_getopt_list_free(a_files);
   a_files = NULL;
   ecore_getopt_list_free(i_files);
   i_files = NULL;
   ecore_getopt_list_free(groups);
   groups = NULL;


   if (!output_filename)
     return _edje_pick_cleanup(files, NULL, EDJE_PICK_OUT_FILENAME_MISSING,
           a_files, i_files, groups, NULL, NULL);
   /* END   - Read command line args */

   if (cleanup)  /* In case GUI call this just to do parsing */
     return _edje_pick_cleanup(files, NULL, EDJE_PICK_NO_ERROR,
           a_files, i_files, groups, NULL, NULL);

   /* Set output params, return OK */
   *ifs = files;
   *ofn = output_filename;
   return EDJE_PICK_NO_ERROR;
}

static void
_edje_pick_external_dir_update(Edje_File *o, Edje_File *edf)
{
   if (edf->external_dir && edf->external_dir->entries_count)
     {
        /* Add external-dir entries */
        unsigned int total = 0;
        unsigned int base = 0;

        if (o->external_dir)
          base = total = o->external_dir->entries_count;
        else
          o->external_dir = calloc(1, sizeof(*(o->external_dir)));

        total += edf->external_dir->entries_count;

        o->external_dir->entries = realloc(o->external_dir->entries,
                                           total * sizeof(Edje_External_Directory_Entry));

        memcpy(&o->external_dir->entries[base], edf->external_dir->entries,
               edf->external_dir->entries_count *
               sizeof(Edje_External_Directory_Entry));

        o->external_dir->entries_count = total;
     }
}

static Edje_File *
_edje_pick_output_prepare(Edje_File *o, Edje_File *edf, char *name)
{
   /* Allocate and prepare header memory buffer */
   if (!o)
     {
        o = calloc(1, sizeof(Edje_File));
        o->compiler = eina_stringshare_add("edje_cc");
        o->version = edf->version;
        o->minor = edf->minor;
        o->feature_ver = edf->feature_ver;
        o->collection = eina_hash_string_small_new(NULL);

        /* Open output file */
        o->ef = eet_open(name, EET_FILE_MODE_WRITE);
     }
   else
     {
        if (o->version != edf->version)
          {
             EINA_LOG_WARN("Warning: Merging files of various version.\n");
             if (o->version < edf->version)
               o->version = edf->version;
          }

        if (o->minor != edf->minor)
          {
             EINA_LOG_WARN("Warning: Merging files of various minor.\n");
             if (o->minor < edf->minor)
               o->minor = edf->minor;
          }

        if (o->feature_ver != edf->feature_ver)
          {
             EINA_LOG_WARN("Warning: Merging files of various feature_ver.\n");
             if (o->feature_ver < edf->feature_ver)
               o->feature_ver = edf->feature_ver;
          }
     }

   _edje_pick_external_dir_update(o, edf);
   return o;
}

static int
_edje_pick_header_make(Edje_File *out_file , Edje_File *edf, Eina_List *ifs)
{
   Edje_Part_Collection_Directory_Entry *ce;
   Eina_Bool status = EDJE_PICK_NO_ERROR;
   Eina_List *l;
   char *name1 = NULL;


   _edje_cache_file_unref(edf);

   /* Build file header */
   if (context->current_file->append)
     {
        Eina_Iterator *i;
        i = eina_hash_iterator_key_new(edf->collection);
        EINA_ITERATOR_FOREACH(i, name1)  /* Run through all keys */
          {
             Edje_Part_Collection_Directory_Entry *ce_out;

             /* Use ALL groups from this file */
             /* Test that no duplicate-group name for files in append mode */
             /* Done here because we don't read EDC before parse cmd line  */
             /* We SKIP group of file in append-mode if we got this group  */
             /* from file in include mode.                                 */
             if (_group_name_in_other_file(ifs, context->current_file, name1))
               continue; /* Skip group of file in append mode */

             ce = eina_hash_find(edf->collection, name1);
             ce_out = malloc(sizeof(*ce_out));
             memcpy(ce_out, ce, sizeof(*ce_out));

             ce_out->id = context->current_group_id;
             EINA_LOG_INFO("Changing ID of group <%d> to <%d>\n",
                   ce->id, ce_out->id);
             context->current_group_id++;

             eina_hash_direct_add(out_file->collection, ce_out->entry, ce_out);

             /* Add this group to groups to handle for this file */
             context->current_file->groups = eina_list_append(
                   context->current_file->groups, eina_stringshare_add(name1));
          }

        eina_iterator_free(i);
     }
   else
     {
        EINA_LIST_FOREACH(context->current_file->groups, l , name1)
          {
             /* Verify group found then add to ouput file header */
             ce = eina_hash_find(edf->collection, name1);

             if (!ce)
               {
                  EINA_LOG_ERR("Group <%s> was not found in <%s> file.\n",
                         name1, context->current_file->name);
                  status = EDJE_PICK_GROUP_NOT_FOUND;
               }
             else
               {
                  Edje_Part_Collection_Directory_Entry *ce_out;

                  /* Add this groups to hash, with filname pefix for entries */
                  ce_out = malloc(sizeof(*ce_out));

                  memcpy(ce_out, ce, sizeof(*ce_out));

                  ce_out->id = context->current_group_id;
                  EINA_LOG_INFO("Changing ID of group <%d> to <%d>\n",
                        ce->id, ce_out->id);
                  context->current_group_id++;

                  eina_hash_direct_add(out_file->collection,ce_out->entry,
                        ce_out);
               }
          }
     }

   return status;
}

static int
_id_cmp(const void *d1, const void *d2)
{
   /* Find currect ID struct */
   return (((Edje_Pick_Data *) d1)->id.old_id - ((intptr_t) d2));
}

static int
_edje_pick_new_id_get(Eina_List *id_list, int id, Eina_Bool set_used)
{
   if (id >= 0)
     {
        Edje_Pick_Data *p_id = eina_list_search_unsorted(id_list,
                                                         _id_cmp,
                                                         (void *) (intptr_t) id);


        if (p_id)
          {
             if (set_used)
               p_id->id.used = EINA_TRUE;

             return p_id->id.new_id;
          }
     }

   return id;
}

static int
_edje_pick_images_add(Edje_File *edf, Edje_File *o)
{
   char buf[1024];
   int size;
   unsigned int k;
   void *data;
   Eina_Bool status = EDJE_PICK_NO_ERROR;
   static int current_img_id = 0;

   if (edf->image_dir)
     {
        if (!o->image_dir)  /* First time only */
          o->image_dir = calloc(1, sizeof(*(o->image_dir)));

        for (k = 0; k < edf->image_dir->entries_count; k++)
          {  /* Copy Images */
             Edje_Image_Directory_Entry *img = &edf->image_dir->entries[k];

             snprintf(buf, sizeof(buf), "edje/images/%i", img->id);
             VERBOSE(EINA_LOG_INFO("Trying to read <%s>\n", img->entry));
             data = eet_read(edf->ef, buf, &size);
             if (size)
               {  /* Advance image ID and register this in imagelist */
                  Edje_Pick_Data *image = malloc(sizeof(*image));

                  image->filename = eina_stringshare_add(img->entry);
                  image->data = data;
                  image->size = size;
                  image->entry = (void *) img;  /* for output file image dir */
                  image->id.old_id = img->id;
                  img->id = image->id.new_id = current_img_id;
                  image->id.used = EINA_FALSE;

                  VERBOSE(EINA_LOG_INFO("Read image <%s> data <%p> size <%d>\n",
                           buf, image->data, image->size));

                  current_img_id++;
                  context->current_file->imagelist = eina_list_append(
                        context->current_file->imagelist, image);
               }
             else
               {
                  if (img->entry)
                    {
                       EINA_LOG_ERR("Image <%s> was not found in <%s> file.\n",
                             img->entry , context->current_file->name);
                       status = EDJE_PICK_IMAGE_NOT_FOUND;
                    }
                  else
                    {
                       EINA_LOG_ERR("Image entry <%s> was not found in <%s> file.\n", buf , context->current_file->name);
                       status = EDJE_PICK_IMAGE_NOT_FOUND;
                    }
                  /* Should that really be freed? Or is some other handling needed? */
                  free(data);
               }
          }

        if (edf->image_dir->entries)
          {  /* Copy image dir entries of current file */
             k = o->image_dir->entries_count; /* save current entries count */
             o->image_dir->entries_count += edf->image_dir->entries_count;

             /* alloc mem first time  or  re-allocate again (bigger array) */
             o->image_dir->entries = realloc(o->image_dir->entries,
                   o->image_dir->entries_count *
                   sizeof(Edje_Image_Directory_Entry));

             /* Concatinate current file entries to re-allocaed array */
             memcpy(&o->image_dir->entries[k], edf->image_dir->entries,
                   edf->image_dir->entries_count *
                   sizeof(Edje_Image_Directory_Entry));
          }

        if (edf->image_dir->sets)
          {  /* Copy image dir sets of current file */
             k = o->image_dir->sets_count;      /* save current sets count */
             o->image_dir->sets_count += edf->image_dir->sets_count;
             /* alloc mem first time  or  re-allocate again (bigger array) */
             o->image_dir->sets = realloc(o->image_dir->sets,
                   o->image_dir->sets_count *
                   sizeof(Edje_Image_Directory_Set_Entry));

             /* Concatinate current file sets to re-allocaed array */
             memcpy(&o->image_dir->sets[k], edf->image_dir->sets,
                   edf->image_dir->sets_count *
                   sizeof(Edje_Image_Directory_Set_Entry));

             for (; k < o->image_dir->sets_count; k++)
               {  /* Fix IDs in sets to new assigned IDs of entries */
                  Eina_List *l;
                  Edje_Image_Directory_Set_Entry *e;
                  Edje_Pick_Data *set = calloc(1, sizeof(*set));
                  set->id.old_id = o->image_dir->sets[k].id;
                  set->id.new_id = k;

                  /* Save IDs in set-list, used in Desc update later */
                  context->current_file->imagesetlist = eina_list_append(
                        context->current_file->imagesetlist, set);

                  o->image_dir->sets[k].id = k;  /* Fix new sets IDs */
                  EINA_LIST_FOREACH(o->image_dir->sets[k].entries, l, e)
                     e->id = _edje_pick_new_id_get(
                           context->current_file->imagelist,
                           e->id, EINA_FALSE);
               }
          }
     }

   return status;
}

static int
_edje_pick_sounds_add(Edje_File *edf)
{
   char buf[1024];
   int size, k;
   void *data;
   Eina_Bool status = EDJE_PICK_NO_ERROR;
   static int current_sample_id = 0;

   if (edf->sound_dir)  /* Copy Sounds */
     {
        for (k = 0; k < (int) edf->sound_dir->samples_count; k++)
          {
             Edje_Sound_Sample *sample = &edf->sound_dir->samples[k];

             snprintf(buf, sizeof(buf), "edje/sounds/%i", sample->id);
             VERBOSE(EINA_LOG_INFO("Trying to read <%s>\n", sample->name));

             data = eet_read(edf->ef, buf, &size);
             if (size)
               {
                  Edje_Pick_Data *smpl = malloc(sizeof(*smpl));
                  smpl->filename = eina_stringshare_add(sample->name);
                  smpl->data = data;
                  smpl->size = size;
                  smpl->entry = (void *) sample; /* for output file sound dir */
                  smpl->id.old_id = sample->id;
                  sample->id = smpl->id.new_id = current_sample_id;
                  smpl->id.used = EINA_FALSE;

                  VERBOSE(EINA_LOG_INFO("Read <%s> sample data <%p> size <%d>\n",
                                 buf, smpl->data, smpl->size));

                  current_sample_id++;
                  context->current_file->samplelist =
                    eina_list_append(context->current_file->samplelist, smpl);
               }
             else
               {
                  EINA_LOG_ERR("Sample <%s> was not found in <%s> file.\n",
                         sample->name, context->current_file->name);
                  status = EDJE_PICK_SAMPLE_NOT_FOUND;
                  /* Should that really be freed? Or is some other handling needed? */
                  free(data);
               }
          }

        for (k = 0; k < (int) edf->sound_dir->tones_count; k++)
          {
             /* Save all tones as well */
             Edje_Pick_Tone *t = malloc(sizeof(*t));

             t->tone = &edf->sound_dir->tones[k];
             /* Update ID to new ID */
             t->tone->id = _edje_pick_new_id_get(context->current_file->samplelist,   /* From samplelist */
                                                 t->tone->id, EINA_FALSE);

             t->used = EINA_FALSE;
             context->current_file->tonelist = eina_list_append(context->current_file->tonelist, t);
          }
     }

   return status;
}

static int
_font_cmp(const void *d1, const void *d2)
{
   const Edje_Font *f1 = d1;
   const Edje_Font *f2 = d2;

   /* Same font if (d1->name == d2->name) AND (d1->file == d2->file) */
   return (strcmp(f1->name, f2->name) |
           strcmp(f1->file, f2->file));
}

static int
_Edje_Pick_Fonts_add(Edje_File *edf)
{
   Eet_Data_Descriptor *_font_list_edd = NULL;
   Eet_Data_Descriptor *_font_edd;
   Edje_Font_List *fl;
   Edje_Font *f;
   Eina_List *l;

   _edje_data_font_list_desc_make(&_font_list_edd, &_font_edd);
   fl = eet_data_read(edf->ef, _font_list_edd, "edje_source_fontmap");

   if (fl)
     {
        EINA_LIST_FOREACH(fl->list, l, f)
          {
             if (!eina_list_search_unsorted(context->fontlist,
                      _font_cmp, f))
               {
                  /* Add only fonts that are NOT regestered in our list */
                  Edje_Pick_Font *ft =  malloc(sizeof(*ft));
                  Edje_Font *st = malloc(sizeof(*st));

                  st->name = (char *) eina_stringshare_add(f->name);
                  st->file = (char *) eina_stringshare_add(f->file);

                  ft->f = st;
                  ft->used = EINA_TRUE;  /* TODO: Fix this later */
                  context->fontlist = eina_list_append(context->fontlist, ft);
               }
          }

        free(fl);
     }

   eet_data_descriptor_free(_font_list_edd);
   eet_data_descriptor_free(_font_edd);

   return EDJE_PICK_NO_ERROR;
}

static int
_edje_pick_scripts_add(Edje_File *edf, int id, int new_id)
{
   int size;
   void *data;
   char buf[1024];

   /* Copy Script */
   snprintf(buf, sizeof(buf), "edje/scripts/embryo/compiled/%i", id);
   data = eet_read(edf->ef, buf, &size);
   if (size)
     {
        Edje_Pick_Data *s = calloc(1, sizeof(*s));

        s->data = data;
        s->size = size;
        s->id.old_id = id;
        s->id.new_id = new_id;
        s->id.used = EINA_TRUE;

        VERBOSE(EINA_LOG_INFO("Read embryo script <%s> data <%p> size <%d>\n",
                       buf, s->data, s->size));
        context->current_file->scriptlist = eina_list_append(context->current_file->scriptlist, s);
     }
   else
     {
        /* Should that really be freed? Or is some other handling needed? */
        free(data);
     }

   return EDJE_PICK_NO_ERROR;
}

static int
_edje_pick_lua_scripts_add(Edje_File *edf, int id, int new_id)
{
   int size;
   void *data;
   char buf[1024];

   /* Copy Script */
   snprintf(buf, sizeof(buf), "edje/scripts/lua/%i", id);
   data = eet_read(edf->ef, buf, &size);
   if (size)
     {
        Edje_Pick_Data *s = calloc(1, sizeof(*s));

        s->data = data;
        s->size = size;
        s->id.old_id = id;
        s->id.new_id = new_id;
        s->id.used = EINA_TRUE;

        VERBOSE(EINA_LOG_INFO("Read lua script <%s> data <%p> size <%d>\n",
                       buf, s->data, s->size));
        context->current_file->luascriptlist = eina_list_append(context->current_file->luascriptlist, s);
     }
   else
     {
        /* Should that really be freed? Or is some other handling needed? */
        free(data);
     }

   return EDJE_PICK_NO_ERROR;
}

static void
_edje_pick_styles_update(Edje_File *o, Edje_File *edf)
{
   /* Color Class in Edje_File */
   Eina_List *l;
   Edje_Style *stl;

   EINA_LIST_FOREACH(edf->styles, l, stl)
     o->styles = eina_list_append(o->styles, stl);
}

static void
_edje_pick_color_class_update(Edje_File *o, Edje_File *edf)
{
   /* Color Class in Edje_File */
   Eina_List *l;
   Edje_Color_Class *cc;

   EINA_LIST_FOREACH(edf->color_classes, l, cc)
     o->color_classes = eina_list_append(o->color_classes, cc);
}

static void
_edje_pick_images_desc_update(Edje_Part_Description_Image *desc)
{
   /* Update all IDs of images in descs */
   if (desc)
     {
        unsigned int k;
        int new_id = (desc->image.set) ?
           _edje_pick_new_id_get(context->current_file->imagesetlist,
                 desc->image.id,
                 EINA_TRUE) :
           _edje_pick_new_id_get(context->current_file->imagelist,
                 desc->image.id,
                 EINA_TRUE);

        desc->image.id = new_id;

        for (k = 0; k < desc->image.tweens_count; k++)
          {
             new_id = (desc->image.set) ?
                _edje_pick_new_id_get(context->current_file->imagesetlist,
                      desc->image.tweens[k]->id ,
                      EINA_TRUE) :
                _edje_pick_new_id_get(context->current_file->imagelist,
                      desc->image.tweens[k]->id ,
                      EINA_TRUE);

             desc->image.tweens[k]->id = new_id;
          }
     }
}

static void
_edje_pick_images_process(Edje_Part_Collection *edc)
{
   /* Find what images are used, update IDs, mark as USED */
   unsigned int i;

   for (i = 0; i < edc->parts_count; i++)
     {
        /* Scan all parts, locate what images used */
        Edje_Part *part = edc->parts[i];

        if (part->type == EDJE_PART_TYPE_IMAGE)
          {
             /* Update IDs of all images in ALL descs of this part */
             unsigned int k;

             _edje_pick_images_desc_update((Edje_Part_Description_Image *) part->default_desc);

             for (k = 0; k < part->other.desc_count; k++)
               _edje_pick_images_desc_update((Edje_Part_Description_Image *) part->other.desc[k]);
          }
     }
}

static int
_sample_cmp(const void *d1, const void *d2)
{
   /* Locate sample by name */
   if (d2)
     {
        Edje_Sound_Sample *sample = ((Edje_Pick_Data *) d1)->entry;

        return strcmp(sample->name, d2);
     }

   return 1;
}

static int
_tone_cmp(const void *d1, const void *d2)
{
   /* Locate tone by name */
   if (d2)
     {
        Edje_Sound_Tone *tone = ((Edje_Pick_Tone *) d1)->tone;

        return strcmp(tone->name, d2);
     }

   return 1;
}

static void
_edje_pick_program_update(Edje_Program *prog)
{
   Edje_Pick_Data *p;
   Edje_Pick_Tone *t;

   /* Scan for used samples, update samples IDs */
   p = eina_list_search_unsorted(context->current_file->samplelist,
                                 (Eina_Compare_Cb) _sample_cmp,
                                 prog->sample_name);

   /* Sample is used by program, should be saved */
   if (p)
     p->id.used = EINA_TRUE;

   /* handle tones as well */
   t = eina_list_search_unsorted(context->current_file->tonelist,
                                 (Eina_Compare_Cb) _tone_cmp,
                                 prog->tone_name);

   /* Tone is used by program, should be saved */
   if (t)
     t->used = EINA_TRUE;
}

static int
_edje_pick_programs_process(Edje_Part_Collection *edc)
{
   /* This wil mark which samples are used and should be saved */
   unsigned int i;

   for(i = 0; i < edc->programs.fnmatch_count; i++)
     _edje_pick_program_update(edc->programs.fnmatch[i]);

   for(i = 0; i < edc->programs.strcmp_count; i++)
     _edje_pick_program_update(edc->programs.strcmp[i]);

   for(i = 0; i < edc->programs.strncmp_count; i++)
     _edje_pick_program_update(edc->programs.strncmp[i]);

   for(i = 0; i < edc->programs.strrncmp_count; i++)
     _edje_pick_program_update(edc->programs.strrncmp[i]);

   for(i = 0; i < edc->programs.nocmp_count; i++)
     _edje_pick_program_update(edc->programs.nocmp[i]);

   return EDJE_PICK_NO_ERROR;
}

static int
_edje_pick_collection_process(Edje_Part_Collection *edc)
{
   /* Update all IDs, NAMES in current collection */
   static int current_collection_id = 0;

   edc->id = current_collection_id;
   current_collection_id++;
   _edje_pick_images_process(edc);
   _edje_pick_programs_process(edc);

   return EDJE_PICK_NO_ERROR;
}

static void
_edje_pick_sound_dir_compose(Eina_List *samples, Eina_List *tones, Edje_File *o)
{  /* Compose sound_dir array from all used samples, tones */
   if (samples)
     {
        Edje_Sound_Sample *sample;
        Edje_Sound_Sample *p;
        Eina_List *l;

        o->sound_dir = calloc(1, sizeof(*(o->sound_dir)));
        o->sound_dir->samples = malloc(eina_list_count(samples) *
                                       sizeof(Edje_Sound_Sample));

        p = o->sound_dir->samples;
        EINA_LIST_FOREACH(samples, l, sample)
          {
             memcpy(p, sample, sizeof(Edje_Sound_Sample));
             p++;
          }

        o->sound_dir->samples_count = eina_list_count(samples);

        if (tones)
          {
             Edje_Sound_Tone *tone;
             Edje_Sound_Tone *t;

             o->sound_dir->tones = malloc(eina_list_count(tones) *
                                          sizeof(Edje_Sound_Tone));

             t = o->sound_dir->tones;
             EINA_LIST_FOREACH(tones, l, tone)
               {
                  memcpy(t, tone, sizeof(Edje_Sound_Tone));
                  t++;
               }

             o->sound_dir->tones_count = eina_list_count(tones);
          }
     }
}

static Eina_List *
_groups_list_get(Edje_File *edf)
{  /* List freed by caller */
   Eina_Iterator *i;
   char *name = NULL;
   Eina_List *groups = NULL;

   i = eina_hash_iterator_key_new(edf->collection);
   EINA_ITERATOR_FOREACH(i, name)
     {  /* Add groups to output list */
        if (name)
          groups = eina_list_append(groups, name);
     }

   eina_iterator_free(i);
   return groups;
}

static Eina_List *
_images_list_get(Edje_File *edf)
{  /* List and content freed by caller */
   Eina_List *images = NULL;

   if (edf->image_dir)
     {  /* Needs to be freed by caller */
        unsigned int k;
        for (k = 0; k < edf->image_dir->entries_count; k++)
          {
             Edje_Image_Directory_Entry *img = &edf->image_dir->entries[k];
             if (img->entry)
               {
                  image_info_ex *ex = calloc(1, sizeof(*ex));
                  ex->name = eina_stringshare_add(img->entry);
                  ex->id = img->id;
                  images = eina_list_append(images, ex);
               }
             else
               EINA_LOG_WARN("Warning: Image name is NULL ID <%d>.\n",
                     img->id);
          }
     }

   return images;
}

static Eina_List *
_samples_list_get(Edje_File *edf)
{  /* List and content freed by caller */
   Eina_List *samples = NULL;

   if (edf->sound_dir)
     {  /* Needs to be freed by caller */
        unsigned int k;
        for (k = 0; k < edf->sound_dir->samples_count; k++)
          {
             Edje_Sound_Sample *sample = &edf->sound_dir->samples[k];
             if (sample->name)
               {
                  image_info_ex *ex = calloc(1, sizeof(*ex));
                  ex->name = eina_stringshare_add(sample->name);
                  ex->id = sample->id;
                  samples = eina_list_append(samples, ex);
               }
             else
               EINA_LOG_WARN("Warning: Sample name is NULL ID <%d>.\n",
                     sample->id);
          }
     }

   return samples;
}

static Eina_List *
_fonts_list_get(Edje_File *edf)
{
   Eet_Data_Descriptor *_font_list_edd = NULL;
   Eet_Data_Descriptor *_font_edd;
   Edje_Font_List *fl;
   Edje_Font *f;
   Eina_List *l;
   Eina_List *fonts = NULL;

   _edje_data_font_list_desc_make(&_font_list_edd, &_font_edd);
   fl = eet_data_read(edf->ef, _font_list_edd, "edje_source_fontmap");

   if (fl)
     {
        EINA_LIST_FOREACH(fl->list, l, f)
          {
             if (f->name && f->file)
               {
                  font_info_ex *st = malloc(sizeof(*st));

                  st->name = (char *) eina_stringshare_add(f->name);
                  st->file = (char *) eina_stringshare_add(f->file);

                  fonts = eina_list_append(fonts, st);
               }
          }

        free(fl);
     }

   eet_data_descriptor_free(_font_list_edd);
   eet_data_descriptor_free(_font_edd);

   return fonts;
}

EAPI int
edje_pick_file_info_read(const char *file_name,
      Eina_List **groups, Eina_List **images,
      Eina_List **samples, Eina_List **fonts)
{  /* All lists and content freed by caller */
   if (file_name)
     {  /* Got file name, read goupe names and add to genlist */
        Edje_File *edf;
        Eet_File *ef = eet_open(file_name, EET_FILE_MODE_READ);
        if (!ef)
          return EDJE_PICK_FILE_OPEN_FAILED;

        edf = eet_data_read(ef, _edje_edd_edje_file, "edje/file");
        if (!edf)
          {
             eet_close(ef);
             return EDJE_PICK_FILE_READ_FAILED;
          }

        if (!(edf->collection))
          {  /* This will handle the case of empty, corrupted file */
             _edje_cache_file_unref(edf);
             eet_close(ef);
             return EDJE_PICK_FILE_COLLECTION_EMPTY;
          }

        *groups = _groups_list_get(edf);

        if ((*groups) == NULL)
          {  /* Just to be on the safe side */
             _edje_cache_file_unref(edf);
             eet_close(ef);
             return EDJE_PICK_FILE_COLLECTION_EMPTY;
          }

        *images = _images_list_get(edf);
        *samples = _samples_list_get(edf);
        *fonts = _fonts_list_get(edf);

        _edje_cache_file_unref(edf);
        eet_close(ef);

        return EDJE_PICK_NO_ERROR;
     }

   return EDJE_PICK_FILE_OPEN_FAILED;
}

EAPI int
edje_pick_process(int argc, char **argv)
{
   char *name1, *output_filename = NULL;
   Eina_List *inp_files = NULL;
   int comp_mode = EET_COMPRESSION_DEFAULT;
   Edje_File *out_file = NULL;
   Eina_List *images = NULL;
   Eina_List *samples = NULL;
   Eina_List *tones = NULL;
   Edje_Image_Directory_Set *sets = NULL; /* ALL files sets composed here */

   Edje_Part_Collection *edc;
   Edje_Part_Collection_Directory_Entry *ce;
   Eet_File *ef;
   Edje_Font_List *fl;
   Eina_List *f, *l;
   char buf[1024];
   char *err = NULL;
   void *n;
   int k, bytes;

   k = edje_pick_command_line_parse(argc, argv,
         &inp_files, &output_filename, EINA_FALSE);

   if ( k != EDJE_PICK_NO_ERROR)
     {
        if (err)
          EINA_LOG_ERR("%s", err);

        return k;
     }

   _edje_pick_args_show(inp_files, output_filename);

   /* START - Main loop scanning input files */
   EINA_LIST_FOREACH(inp_files, f, context->current_file)
     {
        Edje_File *edf;

        ef = eet_open(context->current_file->name, EET_FILE_MODE_READ);
        if (!ef)
          return _edje_pick_cleanup(inp_files, out_file,
                EDJE_PICK_FAILED_OPEN_INP, NULL, NULL, NULL, NULL, NULL);

        edf = eet_data_read(ef, _edje_edd_edje_file, "edje/file");
        if (!edf)
          return _edje_pick_cleanup(inp_files, out_file,
                EDJE_PICK_FAILED_READ_INP, NULL, NULL, NULL, NULL, NULL);

        context->current_file->edf = edf;
        edf->ef = ef;

        out_file = _edje_pick_output_prepare(out_file, edf, output_filename);

        k = _edje_pick_header_make(out_file, edf, inp_files);
        if (k != EDJE_PICK_NO_ERROR)
          {
             eet_close(ef);
             return _edje_pick_cleanup(inp_files, out_file,
                   k, NULL, NULL, NULL, NULL, NULL);
          }

        /* Build lists of all images, samples and fonts of input files    */
        _edje_pick_images_add(edf, out_file);  /* Add Images to imagelist */
        _edje_pick_sounds_add(edf);  /* Add Sounds to samplelist          */
        _Edje_Pick_Fonts_add(edf);   /* Add fonts from file to fonts list */

        /* Copy styles, color class */
        _edje_pick_styles_update(out_file, edf);
        _edje_pick_color_class_update(out_file, edf);

        /* Process Groups */
        EINA_LIST_FOREACH(context->current_file->groups, l , name1)
          {  /* Read group info */
             ce = eina_hash_find(edf->collection, name1);
             if (!ce || (ce->id < 0))
               {
                  EINA_LOG_ERR("Failed to find group <%s> id\n", name1);
                  return _edje_pick_cleanup(inp_files, out_file,
                                            EDJE_PICK_GROUP_NOT_FOUND,
                                            NULL, NULL, NULL, NULL, NULL);
               }

             VERBOSE(EINA_LOG_INFO("Copy group: <%s>\n", name1));

             edje_cache_emp_alloc(ce);

             snprintf(buf, sizeof(buf), "edje/collections/%i", ce->id);
             EINA_LOG_INFO("Trying to read group <%s>\n", buf);
             edc = eet_data_read(edf->ef, _edje_edd_edje_part_collection, buf);
             if (!edc)
               {
                  EINA_LOG_ERR("Failed to read group <%s> id <%d>\n", name1, ce->id);
                  return _edje_pick_cleanup(inp_files, out_file,
                        EDJE_PICK_GROUP_NOT_FOUND,
                        NULL, NULL, NULL, NULL, NULL);
               }

             /* Update IDs */
             _edje_pick_collection_process(edc);

             /* Build lists of all scripts with new IDs */
             _edje_pick_scripts_add(edf, ce->id, edc->id);
             _edje_pick_lua_scripts_add(edf, ce->id, edc->id);

             {
                /* Write the group to output file using new id */
                snprintf(buf, sizeof(buf),
                         "edje/collections/%i", edc->id);
                bytes = eet_data_write(out_file->ef,
                                       _edje_edd_edje_part_collection,
                                       buf, edc, comp_mode);
                EINA_LOG_INFO("Wrote <%d> bytes for group <%s>\n", bytes,buf);
             }

             free(edc);
             edje_cache_emp_free(ce);
             eet_close(ef);
          }

        /* We SKIP writing source, just can't compose it */
        /* FIXME: use Edje_Edit code to generate source */
     } /* END   - Main loop scanning input files */

   if (context->current_group_id == 0)
     {  /* No groups were fetch from input files - ABORT */
        return _edje_pick_cleanup(inp_files, out_file,
              EDJE_PICK_NO_GROUP,
              NULL, NULL, NULL, NULL, NULL);
     }

   /* Write rest of output */

   EINA_LIST_FOREACH(inp_files, f, context->current_file)
     {
        /* Write Scripts from ALL files */
        Edje_Pick_Data *s;
        Edje_Pick_Tone *tn;
        Eina_List *t;

        EINA_LIST_FOREACH(context->current_file->scriptlist, t, s)
          {
             /* Write Scripts */
             snprintf(buf, sizeof(buf),
                      "edje/scripts/embryo/compiled/%i", s->id.new_id);
             VERBOSE(EINA_LOG_INFO("wrote embryo scr <%s> data <%p> size <%d>\n",
                            buf, s->data, s->size));
             eet_write(out_file->ef, buf, s->data, s->size, comp_mode);
          }

        EINA_LIST_FOREACH(context->current_file->luascriptlist, t, s)
          {
             /* Write Lua Scripts */
             snprintf(buf, sizeof(buf),
                      "edje/scripts/lua/%i", s->id.new_id);
             VERBOSE(EINA_LOG_INFO("wrote lua scr <%s> data <%p> size <%d>\n",
                            buf, s->data, s->size));
             eet_write(out_file->ef, buf, s->data, s->size, comp_mode);
          }

        EINA_LIST_FOREACH(context->current_file->imagelist, t, s)
          {
             if (context->current_file->append || s->id.used)
               {
                  snprintf(buf, sizeof(buf), "edje/images/%i", s->id.new_id);
                  eet_write(out_file->ef, buf, s->data, s->size, EINA_TRUE);
                  VERBOSE(EINA_LOG_INFO("Wrote <%s> image data <%p> size <%d>\n", buf, s->data, s->size));
               }
          }

        EINA_LIST_FOREACH(context->current_file->samplelist, l, s)
          {
             if (context->current_file->append || s->id.used)
               {  /* Write only used samples */
                  samples = eina_list_append(samples, s->entry);

                  snprintf(buf, sizeof(buf), "edje/sounds/%i",
                           s->id.new_id);
                  eet_write(out_file->ef, buf,
                            s->data, s->size,EINA_TRUE);
                  VERBOSE(EINA_LOG_INFO("Wrote <%s> sample data <%p> size <%d>\n",
                                 buf, s->data, s->size));
               }
          }

        EINA_LIST_FOREACH(context->current_file->tonelist, l, tn)
          {
             if (context->current_file->append || tn->used)
               tones = eina_list_append(tones, tn->tone);
          }
     }

   _edje_pick_sound_dir_compose(samples, tones, out_file);

   /* Write file header after processing all groups */
   if (out_file)
     {
        bytes = eet_data_write(out_file->ef, _edje_edd_edje_file, "edje/file",
                               out_file, comp_mode);
        VERBOSE(EINA_LOG_INFO("Wrote <%d> bytes for file header.\n", bytes));
     }

   eina_list_free(images);
   eina_list_free(samples);
   eina_list_free(tones);

   fl = calloc(1, sizeof(*fl));

   EINA_LIST_FOREACH(context->fontlist, l, n)
     {
        /*  Create a font list from used fonts */
        Edje_Pick_Font *fnt = n;
        if (context->current_file->append || fnt->used)
          fl->list = eina_list_append(fl->list, fnt->f);
     }

   if (out_file)
     {
        /* Write Fonts from all files */
        Eet_Data_Descriptor *_font_list_edd = NULL;
        Eet_Data_Descriptor *_font_edd;

        _edje_data_font_list_desc_make(&_font_list_edd, &_font_edd);
        bytes = eet_data_write(out_file->ef, _font_list_edd,
                               "edje_source_fontmap", fl, comp_mode);
        VERBOSE(EINA_LOG_INFO("Wrote <%d> bytes for fontmap.\n", bytes));

        eet_data_descriptor_free(_font_list_edd);
        eet_data_descriptor_free(_font_edd);
     }

   free(fl);

   if (sets)
     free(sets);

   if (output_filename)
     printf("Wrote <%s> output file.\n", output_filename);

   return _edje_pick_cleanup(inp_files, out_file, EDJE_PICK_NO_ERROR,
         NULL, NULL, NULL, NULL, NULL);
}

typedef void (*_finish_cb)();

static Eina_Bool _play_finished(void *data, Eo *in, const Eo_Event_Description *desc EINA_UNUSED, void *event_info EINA_UNUSED)
{
   printf("<%s> <%d>\n", __func__, __LINE__);
   eo_del(in);
   _finish_cb finish = data;
   finish();

   return EINA_TRUE;
}

EAPI Eina_Bool
edje_pick_sample_play(void *data, const char *id, size_t size, const double speed, void *finish (void))
{
   _edje_multisense_init();
   return _edje_multisense_internal_sound_sample_play_ex(data, id, size, speed, _play_finished, finish);
}

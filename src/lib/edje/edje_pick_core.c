#include "Edje_Pick.h"
#include "edje_private.h"

static unsigned char *init_count = 0;

struct _Edje_Pick_Id
{
   int old_id;
   int new_id;
   Eina_Bool used;
};
typedef struct _Edje_Pick_Id Edje_Pick_Id;

struct _Edje_Pick_File_Info
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
typedef struct _Edje_Pick_File_Info Edje_Pick_File_Info;

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

struct _Edje_Pick_Session
{
   Eina_Bool v; /* Verbose */
   Eina_List *input_files_infos; /* List of Edje_Pick_File_Info * */
   Edje_Pick_File_Info output_info;
   Eina_List *fontlist;
   Eina_Hash *groups_hash;
   int next_group_id;
   int next_img_id;
   int next_sample_id;
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

#define VERBOSE(COMMAND) if (session->v) { COMMAND; }

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

EAPI Edje_Pick_Session *
edje_pick_session_new()
{
   Edje_Pick_Session *session = calloc(1, sizeof(Edje_Pick_Session));
   session->groups_hash = eina_hash_stringshared_new(NULL);
   return session;
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

EAPI void
edje_pick_session_del(Edje_Pick_Session *session)
{
   Edje_Pick_File_Info *info;
   _edje_pick_out_file_free(session->output_info.edf);

   EINA_LIST_FREE(session->input_files_infos, info)
     {
        char *name;
        EINA_LIST_FREE(info->groups, name)
          eina_stringshare_del(name);

        _edje_pick_data_free(info->scriptlist);
        info->scriptlist = NULL;

        _edje_pick_data_free(info->luascriptlist);
        info->luascriptlist = NULL;

        _edje_pick_data_free(info->imagelist);
        info->imagelist = NULL;

        _edje_pick_data_free(info->imagesetlist);
        info->imagesetlist = NULL;

        _edje_pick_data_free(info->samplelist);

        EINA_LIST_FREE(info->tonelist, name)
          free(name);

        if (info->edf)
          _edje_cache_file_unref(info->edf);

        free(info);
     }

   Edje_Pick_Font *ft;
   EINA_LIST_FREE(session->fontlist, ft)
     {
        Edje_Font *st = ft->f;

        eina_stringshare_del(st->name);
        eina_stringshare_del(st->file);
        free(st);
        free(ft);
     }
   free(session);
}

EAPI Eina_Bool
edje_pick_input_file_add(Edje_Pick_Session *session, const char *name)
{
   Edje_Pick_File_Info *info = calloc(1, sizeof(*info));
   info->append = EINA_TRUE;
   info->name = eina_stringshare_add(name);
   session->input_files_infos = eina_list_append(session->input_files_infos, info);
   return EINA_TRUE;
}

EAPI Eina_Bool
edje_pick_output_file_add(Edje_Pick_Session *session, const char *name)
{
   session->output_info.name = eina_stringshare_add(name);
   return EINA_TRUE;
}

/* Look for group name in all input files that are not d1 */
static Eina_Bool
_is_group_already_present(Edje_Pick_Session *session, const char *group_name)
{
   return !!eina_hash_find(session->groups_hash, group_name);
}

EAPI Eina_Bool
edje_pick_group_add(Edje_Pick_Session *session, const char *in_file, const char *in_group)
{
   Eina_List *itr;
   Edje_Pick_File_Info *info, *in_info = NULL;
   const char *in_str = eina_stringshare_add(in_file);
   if (_is_group_already_present(session, in_group))
      return EINA_FALSE;
   EINA_LIST_FOREACH(session->input_files_infos, itr, info)
     {
        if (!strcmp(info->name, in_str))
          {
             in_info = info;
             break;
          }
     }
   if (!in_info)
     {
        in_info = calloc(1, sizeof(*in_info));
        in_info->name = in_str;
        session->input_files_infos = eina_list_append(session->input_files_infos, in_info);
     }
   in_info->groups = eina_list_append(in_info->groups, eina_stringshare_add(in_group));
   eina_hash_add(session->groups_hash, in_group, &itr); // the data is not important, just not NULL
   return EINA_TRUE;
}

static void
_session_print(const Edje_Pick_Session *session)
{  /* Print command-line arguments after parsing phase */
   Edje_Pick_File_Info *info;
   Eina_List *itr;
   char *group_name;

   EINA_LOG_INFO("Got infos for <%d> input files.\n", eina_list_count(session->input_files_infos));

   EINA_LIST_FOREACH(session->input_files_infos, itr, info)
     {
        if (info->append)
          printf("\nFile name: %s\n\tGroups: ALL (append mode)\n", info->name);
        else
          {
             Eina_List *itr2;
             printf("\nFile name: %s\n\tGroups:\n", info->name);
             EINA_LIST_FOREACH(info->groups, itr2, group_name)
                printf("\t\t%s\n", group_name);
          }
     }

   EINA_LOG_INFO("\nOutput file name was <%s>\n", session->output_info.name);
}

EAPI const char *
edje_pick_error_get_as_string(Edje_Pick_Status s)
{
   switch (s)
     {
      case EDJE_PICK_NO_ERROR:
         return NULL;
      case EDJE_PICK_OUT_FILENAME_MISSING:
         return "Output file name missing.";
      case EDJE_PICK_FAILED_OPEN_INP:
         return "Failed to open input file.";
      case EDJE_PICK_FAILED_READ_INP:
         return "Failed to read input file.";
      case EDJE_PICK_FAILED_ADD_GROUP:
         return "Failed to add group.";
      case EDJE_PICK_NO_GROUP:
         return "No groups to fetch from input files.";
      case EDJE_PICK_INCLUDE_MISSING:
         return "Cannot select groups when no input file included.";
      case EDJE_PICK_GROUP_MISSING:
         return "Group name missing for include file.";
      case EDJE_PICK_PARSE_FAILED:
         return "Command parsing failed.";
      case EDJE_PICK_FILE_OPEN_FAILED:
         return "Failed to open file.";
      case EDJE_PICK_FILE_READ_FAILED:
         return "Failed to read file.";
      case EDJE_PICK_FILE_COLLECTION_EMPTY:
         return "File collection is empty (corrupted?).";
      default:
         return "Undefined Status";
     }
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

static void
_session_output_prepare(Edje_Pick_Session *session, Edje_Pick_File_Info *in_info)
{
   Edje_File *edf = in_info->edf;
   Edje_File *o = session->output_info.edf;
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
        o->ef = eet_open(session->output_info.name, EET_FILE_MODE_WRITE);
        session->output_info.edf = o;
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
}

static Edje_Pick_Status
_header_make(Edje_Pick_Session *session, Edje_Pick_File_Info *in_info)
{
   Edje_Part_Collection_Directory_Entry *ce;
   Eina_Bool status = EDJE_PICK_NO_ERROR;
   char *group_name = NULL;
   Edje_File *edf = in_info->edf;

   _edje_cache_file_unref(edf);

   /* Build file header */
   if (in_info->append)
     {
        Eina_Iterator *i = eina_hash_iterator_key_new(edf->collection);
        EINA_ITERATOR_FOREACH(i, group_name)  /* Run through all keys */
          {
             Edje_Part_Collection_Directory_Entry *ce_out;

             /* Use ALL groups from this file */
             /* Test that no duplicate-group name for files in append mode */
             /* Done here because we don't read EDC before parse cmd line  */
             /* We SKIP group of file in append-mode if we got this group  */
             /* from file in include mode.                                 */
             if (_is_group_already_present(session, group_name))
               continue; /* Skip group of file in append mode */

             ce = eina_hash_find(edf->collection, group_name);
             ce_out = malloc(sizeof(*ce_out));
             memcpy(ce_out, ce, sizeof(*ce_out));

             ce_out->id = session->next_group_id;
             EINA_LOG_INFO("Changing ID of group <%d> to <%d>\n",
                   ce->id, ce_out->id);
             session->next_group_id++;

             eina_hash_direct_add(session->output_info.edf->collection, ce_out->entry, ce_out);

             /* Add this group to groups to handle for this file */
             edje_pick_group_add(session, in_info->name, group_name);
          }

        eina_iterator_free(i);
     }
   else
     {
        Eina_List *itr;
        EINA_LIST_FOREACH(in_info->groups, itr , group_name)
          {
             /* Verify group found then add to ouput file header */
             ce = eina_hash_find(edf->collection, group_name);

             if (!ce)
               {
                  EINA_LOG_ERR("Group <%s> was not found in <%s> file.\n",
                         group_name, in_info->name);
                  status = EDJE_PICK_GROUP_NOT_FOUND;
               }
             else
               {
                  Edje_Part_Collection_Directory_Entry *ce_out;

                  /* Add this groups to hash, with filname pefix for entries */
                  ce_out = malloc(sizeof(*ce_out));

                  memcpy(ce_out, ce, sizeof(*ce_out));

                  ce_out->id = session->next_group_id;
                  EINA_LOG_INFO("Changing ID of group <%d> to <%d>\n",
                        ce->id, ce_out->id);
                  session->next_group_id++;

                  eina_hash_direct_add(session->output_info.edf->collection,ce_out->entry, ce_out);
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

static Edje_Pick_Status
_images_add(Edje_Pick_Session *session, Edje_Pick_File_Info *in_info)
{
   char buf[100];
   int size;
   unsigned int k;
   Eina_Bool status = EDJE_PICK_NO_ERROR;
   Edje_File *edf = in_info->edf;
   Edje_File *o = session->output_info.edf;

   if (edf->image_dir)
     {
        if (!o->image_dir)  /* First time only */
          o->image_dir = calloc(1, sizeof(*(o->image_dir)));

        for (k = 0; k < edf->image_dir->entries_count; k++)
          {  /* Copy Images */
             Edje_Image_Directory_Entry *img = &edf->image_dir->entries[k];

             snprintf(buf, sizeof(buf), "edje/images/%i", img->id);
             VERBOSE(EINA_LOG_INFO("Trying to read <%s>\n", img->entry));
             void *data = eet_read(edf->ef, buf, &size);
             if (size)
               {  /* Advance image ID and register this in imagelist */
                  Edje_Pick_Data *image = malloc(sizeof(*image));

                  image->filename = eina_stringshare_add(img->entry);
                  image->data = data;
                  image->size = size;
                  image->entry = (void *) img;  /* for output file image dir */
                  image->id.old_id = img->id;
                  img->id = image->id.new_id = session->next_img_id;
                  image->id.used = EINA_FALSE;

                  VERBOSE(EINA_LOG_INFO("Read image <%s> data <%p> size <%d>\n",
                           buf, image->data, image->size));

                  session->next_img_id++;
                  in_info->imagelist = eina_list_append(
                        in_info->imagelist, image);
               }
             else
               {
                  if (img->entry)
                    {
                       EINA_LOG_ERR("Image <%s> was not found in <%s> file.\n",
                             img->entry , in_info->name);
                    }
                  else
                    {
                       EINA_LOG_ERR("Image entry <%s> was not found in <%s> file.\n", buf , in_info->name);
                    }
                  status = EDJE_PICK_IMAGE_NOT_FOUND;
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
                  in_info->imagesetlist = eina_list_append(
                        in_info->imagesetlist, set);

                  o->image_dir->sets[k].id = k;  /* Fix new sets IDs */
                  EINA_LIST_FOREACH(o->image_dir->sets[k].entries, l, e)
                     e->id = _edje_pick_new_id_get(
                           in_info->imagelist,
                           e->id, EINA_FALSE);
               }
          }
     }

   return status;
}

static Edje_Pick_Status
_sounds_add(Edje_Pick_Session *session, Edje_Pick_File_Info *in_info)
{
   char buf[100];
   int size, k;
   void *data;
   Eina_Bool status = EDJE_PICK_NO_ERROR;
   Edje_File *edf = in_info->edf;

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
                  sample->id = smpl->id.new_id = session->next_sample_id;
                  smpl->id.used = EINA_FALSE;

                  VERBOSE(EINA_LOG_INFO("Read <%s> sample data <%p> size <%d>\n",
                                 buf, smpl->data, smpl->size));

                  session->next_sample_id++;
                  in_info->samplelist =
                    eina_list_append(in_info->samplelist, smpl);
               }
             else
               {
                  EINA_LOG_ERR("Sample <%s> was not found in <%s> file.\n",
                         sample->name, in_info->name);
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
             t->tone->id = _edje_pick_new_id_get(in_info->samplelist,   /* From samplelist */
                                                 t->tone->id, EINA_FALSE);

             t->used = EINA_FALSE;
             in_info->tonelist = eina_list_append(in_info->tonelist, t);
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

static Edje_Pick_Status
_fonts_add(Edje_Pick_Session *session, Edje_Pick_File_Info *in_info)
{
   Eet_Data_Descriptor *_font_list_edd = NULL;
   Eet_Data_Descriptor *_font_edd;
   Edje_Font_List *fl;
   Edje_Font *f;
   Eina_List *l;
   Edje_File *edf = in_info->edf;

   _edje_data_font_list_desc_make(&_font_list_edd, &_font_edd);
   fl = eet_data_read(edf->ef, _font_list_edd, "edje_source_fontmap");

   if (fl)
     {
        EINA_LIST_FOREACH(fl->list, l, f)
          {
             if (!eina_list_search_unsorted(session->fontlist,
                      _font_cmp, f))
               {
                  /* Add only fonts that are NOT regestered in our list */
                  Edje_Pick_Font *ft =  malloc(sizeof(*ft));
                  Edje_Font *st = malloc(sizeof(*st));

                  st->name = (char *) eina_stringshare_add(f->name);
                  st->file = (char *) eina_stringshare_add(f->file);

                  ft->f = st;
                  ft->used = EINA_TRUE;  /* TODO: Fix this later */
                  session->fontlist = eina_list_append(session->fontlist, ft);
               }
          }

        free(fl);
     }

   eet_data_descriptor_free(_font_list_edd);
   eet_data_descriptor_free(_font_edd);

   return EDJE_PICK_NO_ERROR;
}

static Edje_Pick_Status
_scripts_add(Edje_Pick_Session *session, Edje_Pick_File_Info *in_info, int id, int new_id)
{
   int size;
   void *data;
   char buf[100];
   Edje_File *edf = in_info->edf;

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
        in_info->scriptlist = eina_list_append(in_info->scriptlist, s);
     }
   else
     {
        /* Should that really be freed? Or is some other handling needed? */
        free(data);
     }

   return EDJE_PICK_NO_ERROR;
}

static Edje_Pick_Status
_lua_scripts_add(Edje_Pick_Session *session, Edje_Pick_File_Info *in_info, int id, int new_id)
{
   int size;
   void *data;
   char buf[100];
   Edje_File *edf = in_info->edf;

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
        in_info->luascriptlist = eina_list_append(in_info->luascriptlist, s);
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
_edje_pick_images_desc_update(Edje_Pick_File_Info *in_info, Edje_Part_Description_Image *desc)
{
   /* Update all IDs of images in descs */
   if (desc)
     {
        unsigned int k;
        int new_id = (desc->image.set) ?
           _edje_pick_new_id_get(in_info->imagesetlist,
                 desc->image.id,
                 EINA_TRUE) :
           _edje_pick_new_id_get(in_info->imagelist,
                 desc->image.id,
                 EINA_TRUE);

        desc->image.id = new_id;

        for (k = 0; k < desc->image.tweens_count; k++)
          {
             new_id = (desc->image.set) ?
                _edje_pick_new_id_get(in_info->imagesetlist,
                      desc->image.tweens[k]->id ,
                      EINA_TRUE) :
                _edje_pick_new_id_get(in_info->imagelist,
                      desc->image.tweens[k]->id ,
                      EINA_TRUE);

             desc->image.tweens[k]->id = new_id;
          }
     }
}

static void
_edje_pick_images_process(Edje_Pick_File_Info *in_info, Edje_Part_Collection *edc)
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

             _edje_pick_images_desc_update(in_info, (Edje_Part_Description_Image *) part->default_desc);

             for (k = 0; k < part->other.desc_count; k++)
               _edje_pick_images_desc_update(in_info, (Edje_Part_Description_Image *) part->other.desc[k]);
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
_program_update(Edje_Pick_File_Info *in_info, Edje_Program *prog)
{
   Edje_Pick_Data *p;
   Edje_Pick_Tone *t;

   /* Scan for used samples, update samples IDs */
   p = eina_list_search_unsorted(in_info->samplelist,
                                 (Eina_Compare_Cb) _sample_cmp,
                                 prog->sample_name);

   /* Sample is used by program, should be saved */
   if (p)
     p->id.used = EINA_TRUE;

   /* handle tones as well */
   t = eina_list_search_unsorted(in_info->tonelist,
                                 (Eina_Compare_Cb) _tone_cmp,
                                 prog->tone_name);

   /* Tone is used by program, should be saved */
   if (t)
     t->used = EINA_TRUE;
}

static int
_edje_pick_programs_process(Edje_Pick_File_Info *in_info, Edje_Part_Collection *edc)
{
   /* This wil mark which samples are used and should be saved */
   unsigned int i;

   for(i = 0; i < edc->programs.fnmatch_count; i++)
     _program_update(in_info, edc->programs.fnmatch[i]);

   for(i = 0; i < edc->programs.strcmp_count; i++)
     _program_update(in_info, edc->programs.strcmp[i]);

   for(i = 0; i < edc->programs.strncmp_count; i++)
     _program_update(in_info, edc->programs.strncmp[i]);

   for(i = 0; i < edc->programs.strrncmp_count; i++)
     _program_update(in_info, edc->programs.strrncmp[i]);

   for(i = 0; i < edc->programs.nocmp_count; i++)
     _program_update(in_info, edc->programs.nocmp[i]);

   return EDJE_PICK_NO_ERROR;
}

static int
_edje_pick_collection_process(Edje_Pick_File_Info *in_info, Edje_Part_Collection *edc)
{
   /* Update all IDs, NAMES in current collection */
   static int current_collection_id = 0;

   edc->id = current_collection_id;
   current_collection_id++;
   _edje_pick_images_process(in_info, edc);
   _edje_pick_programs_process(in_info, edc);

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
                  Edje_Pick_Image_Info *ex = calloc(1, sizeof(*ex));
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
                  Edje_Pick_Image_Info *ex = calloc(1, sizeof(*ex));
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
                  Edje_Pick_Font_Info *st = malloc(sizeof(*st));

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

EAPI Edje_Pick_Status
edje_pick_process(Edje_Pick_Session *session)
{
   Edje_Pick_File_Info *info;
   char *group_name, *output_filename = NULL;
   int comp_mode = EET_COMPRESSION_DEFAULT;
   Eina_List *samples = NULL;
   Eina_List *tones = NULL;
   Edje_Image_Directory_Set *sets = NULL; /* ALL files sets composed here */

   Edje_Part_Collection *edc;
   Edje_Part_Collection_Directory_Entry *ce;
   Edje_Font_List *fl;
   Eina_List *itr, *itr2;
   char buf[100];
   int bytes;

   _session_print(session);

   /* START - Main loop scanning input files */
   EINA_LIST_FOREACH(session->input_files_infos, itr, info)
     {
        Eet_File *ef = eet_open(info->name, EET_FILE_MODE_READ);
        if (!ef) return EDJE_PICK_FAILED_OPEN_INP;

        info->edf = eet_data_read(ef, _edje_edd_edje_file, "edje/file");
        if (!info->edf) return EDJE_PICK_FAILED_READ_INP;

        info->edf->ef = ef;
        _session_output_prepare(session, info);
        Edje_Pick_Status st = _header_make(session, info);
        if (st != EDJE_PICK_NO_ERROR) return st;

        /* Build lists of all images, samples and fonts of input files    */
        _images_add(session, info);  /* Add Images to imagelist */
        _sounds_add(session, info);  /* Add Sounds to samplelist          */
        _fonts_add(session, info);   /* Add fonts from file to fonts list */

        /* Copy styles, color class */
        _edje_pick_styles_update(session->output_info.edf, info->edf);
        _edje_pick_color_class_update(session->output_info.edf, info->edf);

        /* Process Groups */
        EINA_LIST_FOREACH(info->groups, itr2, group_name)
          {  /* Read group info */
             ce = eina_hash_find(info->edf->collection, group_name);
             if (!ce || (ce->id < 0))
               {
                  EINA_LOG_ERR("Failed to find group <%s>'s id\n", group_name);
                  return EDJE_PICK_GROUP_NOT_FOUND;
               }

             VERBOSE(EINA_LOG_INFO("Copy group: <%s>\n", group_name));

             edje_cache_emp_alloc(ce);

             snprintf(buf, sizeof(buf), "edje/collections/%i", ce->id);
             EINA_LOG_INFO("Trying to read group <%s>\n", buf);
             edc = eet_data_read(info->edf->ef, _edje_edd_edje_part_collection, buf);
             if (!edc)
               {
                  EINA_LOG_ERR("Failed to read group <%s> id <%d>\n", group_name, ce->id);
                  return EDJE_PICK_GROUP_NOT_FOUND;
               }

             /* Update IDs */
             _edje_pick_collection_process(info, edc);

             /* Build lists of all scripts with new IDs */
             _scripts_add(session, info, ce->id, edc->id);
             _lua_scripts_add(session, info, ce->id, edc->id);

             {
                /* Write the group to output file using new id */
                snprintf(buf, sizeof(buf), "edje/collections/%i", edc->id);
                bytes = eet_data_write(session->output_info.edf->ef,
                                       _edje_edd_edje_part_collection,
                                       buf, edc, comp_mode);
                EINA_LOG_INFO("Wrote <%d> bytes for group <%s>\n", bytes, buf);
             }

             free(edc);
             edje_cache_emp_free(ce);
          }

        eet_close(ef);
        info->edf->ef = NULL;
        /* We SKIP writing source, just can't compose it */
        /* FIXME: use Edje_Edit code to generate source */
     } /* END   - Main loop scanning input files */


   if (session->next_group_id == 0)
     {  /* No groups were fetch from input files - ABORT */
        return EDJE_PICK_NO_GROUP;
     }

   /* Write rest of output */

   EINA_LIST_FOREACH(session->input_files_infos, itr, info)
     {
        /* Write Scripts from ALL files */
        Edje_Pick_Data *s;
        Edje_Pick_Tone *tn;

        EINA_LIST_FOREACH(info->scriptlist, itr2, s)
          {
             /* Write Scripts */
             snprintf(buf, sizeof(buf),
                      "edje/scripts/embryo/compiled/%i", s->id.new_id);
             VERBOSE(EINA_LOG_INFO("wrote embryo scr <%s> data <%p> size <%d>\n",
                            buf, s->data, s->size));
             eet_write(session->output_info.edf->ef, buf, s->data, s->size, comp_mode);
          }

        EINA_LIST_FOREACH(info->luascriptlist, itr2, s)
          {
             /* Write Lua Scripts */
             snprintf(buf, sizeof(buf),
                      "edje/scripts/lua/%i", s->id.new_id);
             VERBOSE(EINA_LOG_INFO("wrote lua scr <%s> data <%p> size <%d>\n",
                            buf, s->data, s->size));
             eet_write(session->output_info.edf->ef, buf, s->data, s->size, comp_mode);
          }

        EINA_LIST_FOREACH(info->imagelist, itr2, s)
          {
             if (info->append || s->id.used)
               {
                  snprintf(buf, sizeof(buf), "edje/images/%i", s->id.new_id);
                  eet_write(session->output_info.edf->ef, buf, s->data, s->size, EINA_TRUE);
                  VERBOSE(EINA_LOG_INFO("Wrote <%s> image data <%p> size <%d>\n", buf, s->data, s->size));
               }
          }

        EINA_LIST_FOREACH(info->samplelist, itr2, s)
          {
             if (info->append || s->id.used)
               {  /* Write only used samples */
                  samples = eina_list_append(samples, s->entry);

                  snprintf(buf, sizeof(buf), "edje/sounds/%i",
                           s->id.new_id);
                  eet_write(session->output_info.edf->ef, buf,
                            s->data, s->size,EINA_TRUE);
                  VERBOSE(EINA_LOG_INFO("Wrote <%s> sample data <%p> size <%d>\n",
                                 buf, s->data, s->size));
               }
          }

        EINA_LIST_FOREACH(info->tonelist, itr2, tn)
          {
             if (info->append || tn->used)
               tones = eina_list_append(tones, tn->tone);
          }
     }

   _edje_pick_sound_dir_compose(samples, tones, session->output_info.edf);
   eina_list_free(samples);
   eina_list_free(tones);

   /* Write file header after processing all groups */
   bytes = eet_data_write(session->output_info.edf->ef, _edje_edd_edje_file, "edje/file",
         session->output_info.edf, comp_mode);
   VERBOSE(EINA_LOG_INFO("Wrote <%d> bytes for file header.\n", bytes));

   fl = calloc(1, sizeof(*fl));

   Edje_Pick_Font *fnt;
   EINA_LIST_FOREACH(session->fontlist, itr, fnt)
     {
        /*  Create a font list from used fonts */
        if (fnt->used)
          fl->list = eina_list_append(fl->list, fnt->f);
     }

     {
        /* Write Fonts from all files */
        Eet_Data_Descriptor *_font_list_edd = NULL;
        Eet_Data_Descriptor *_font_edd;

        _edje_data_font_list_desc_make(&_font_list_edd, &_font_edd);
        bytes = eet_data_write(session->output_info.edf->ef, _font_list_edd,
                               "edje_source_fontmap", fl, comp_mode);
        VERBOSE(EINA_LOG_INFO("Wrote <%d> bytes for fontmap.\n", bytes));

        eet_data_descriptor_free(_font_list_edd);
        eet_data_descriptor_free(_font_edd);
     }

   free(fl);

   if (sets) free(sets);

   if (output_filename)
     printf("Wrote <%s> output file.\n", output_filename);

   return EDJE_PICK_NO_ERROR;
}

EAPI void
edje_pick_session_verbose_set(Edje_Pick_Session *session, Eina_Bool verbose)
{
   if (session) session->v = verbose;
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

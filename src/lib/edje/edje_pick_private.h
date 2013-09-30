#ifndef __EDJE_PICK_PRIVATE_H__
#define __EDJE_PICK_PRIVATE_H__
#include "edje_private.h"
#include "Edje_Pick.h"

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

EAPI void _edje_pick_data_free(Eina_List *l);
EAPI void _edje_pick_out_file_free(Edje_File *out_file);
EAPI void _edje_pick_args_show(Eina_List *ifs, char *out);
EAPI int _edje_pick_cleanup(Eina_List *ifs, Edje_File *out_file, Edje_Pick_Status s, Eina_List *a_files, Eina_List *i_files, Eina_List *groups, const char *errfile, const char *errgroup);
EAPI int _group_name_in_other_file(Eina_List *inp_files, void *d1, void *d2);
EAPI void _edje_pick_external_dir_update(Edje_File *o, Edje_File *edf);
EAPI Edje_File * _edje_pick_output_prepare(Edje_File *o, Edje_File *edf, char *name);
EAPI int _edje_pick_header_make(Edje_File *out_file , Edje_File *edf, Eina_List *ifs);
EAPI int _id_cmp(const void *d1, const void *d2);
EAPI int _edje_pick_new_id_get(Eina_List *id_list, int id, Eina_Bool set_used);
EAPI int _edje_pick_images_add(Edje_File *edf, Edje_File *o);
EAPI int _edje_pick_sounds_add(Edje_File *edf);
EAPI int _font_cmp(const void *d1, const void *d2);
EAPI int _Edje_Pick_Fonts_add(Edje_File *edf);
EAPI int _edje_pick_scripts_add(Edje_File *edf, int id, int new_id);
EAPI int _edje_pick_lua_scripts_add(Edje_File *edf, int id, int new_id);
EAPI void _edje_pick_styles_update(Edje_File *o, Edje_File *edf);
EAPI void _edje_pick_color_class_update(Edje_File *o, Edje_File *edf);
EAPI void _edje_pick_images_desc_update(Edje_Part_Description_Image *desc);
EAPI void _edje_pick_images_process(Edje_Part_Collection *edc);
EAPI int _sample_cmp(const void *d1, const void *d2);
EAPI int _tone_cmp(const void *d1, const void *d2);
EAPI void _edje_pick_program_update(Edje_Program *prog);
EAPI int _edje_pick_programs_process(Edje_Part_Collection *edc);
EAPI int _edje_pick_collection_process(Edje_Part_Collection *edc);
EAPI void _edje_pick_sound_dir_compose(Eina_List *samples, Eina_List *tones, Edje_File *o);
#endif

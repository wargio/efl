#ifndef __EDJE_PICK_H__
#define __EDJE_PICK_H__
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <Ecore_Getopt.h>

#ifdef HAVE_EVIL
# include <Evil.h>
#endif

typedef struct _Edje_Pick Edje_Pick;

struct _sample_info_ex
{
   const char *name; /* Name of object */
   int id;     /* the id no. of the sound */
};
typedef struct _sample_info_ex sample_info_ex;
typedef struct _sample_info_ex image_info_ex;

struct _font_info_ex
{
   const char *name;
   const char *file;
};
typedef struct _font_info_ex font_info_ex;

enum _Edje_Pick_Status
  {
    EDJE_PICK_NO_ERROR,
    EDJE_PICK_OUT_FILENAME_MISSING,
    EDJE_PICK_FAILED_OPEN_INP,
    EDJE_PICK_FAILED_READ_INP,
    EDJE_PICK_GROUP_NOT_FOUND,
    EDJE_PICK_IMAGE_NOT_FOUND,
    EDJE_PICK_SAMPLE_NOT_FOUND,
    EDJE_PICK_INCLUDE_MISSING,
    EDJE_PICK_GROUP_MISSING,
    EDJE_PICK_PARSE_FAILED,
    EDJE_PICK_HELP_SHOWN,
    EDJE_PICK_DUP_GROUP,
    EDJE_PICK_NO_GROUP,
    EDJE_PICK_DUP_FILE_ADDED,
    EDJE_PICK_FILE_INCLUDED,
    EDJE_PICK_FILE_OPENED,
    EDJE_PICK_FILE_RENAME_FAILED,
    EDJE_PICK_FILE_OPEN_FAILED,
    EDJE_PICK_FILE_READ_FAILED,
    EDJE_PICK_FILE_COLLECTION_EMPTY
  };
typedef enum _Edje_Pick_Status Edje_Pick_Status;

EAPI void edje_pick_init(void);
EAPI void edje_pick_shutdown(void);
EAPI Edje_Pick *edje_pick_context_new(void);
EAPI void edje_pick_context_free(Edje_Pick *);
EAPI void edje_pick_context_set(Edje_Pick *c);
EAPI const char *edje_pick_err_str_get(Edje_Pick_Status s);
EAPI int edje_pick_file_info_read(const char *file_name, Eina_List **groups, Eina_List **images, Eina_List **samples, Eina_List **fonts);
EAPI int edje_pick_command_line_parse(int argc, char **argv, Eina_List **ifs, char **ofn, Eina_Bool cleanup);
EAPI int edje_pick_process(int argc, char **argv);
EAPI Eina_Bool edje_pick_sample_play(void *data, const char *id, size_t size, const double speed, void *finish (void));
#endif

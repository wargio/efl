#ifndef __EDJE_PICK_H__
#define __EDJE_PICK_H__

#include <Eina.h>
/* FIXME Daniel: Remove these bloody includes from this file!!! */
/*
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
*/

/* FIXME Daniel: see suggestion below, change to Edje_Pick_Session */
typedef struct _Edje_Pick_Session Edje_Pick_Session;

/* FIXME Daniel: maybe union of all the types. Naming convention. */
struct _sample_info_ex
{
   const char *name; /* Name of object */
   int id;     /* the id no. of the sound */
};
typedef struct _sample_info_ex Edje_Pick_Sample_Info;
typedef struct _sample_info_ex Edje_Pick_Image_Info;

struct _font_info_ex
{
   const char *name;
   const char *file;
};
typedef struct _font_info_ex Edje_Pick_Font_Info;

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
    EDJE_PICK_FAILED_ADD_GROUP,
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
EAPI Edje_Pick_Session *edje_pick_session_new();
EAPI void edje_pick_session_del(Edje_Pick_Session *session);

EAPI const char *edje_pick_error_get_as_string(Edje_Pick_Status s);

/* FIXME Daniel: Does it really fill all the needs for gpick? Don't we have a such function already in Edje? */
EAPI int edje_pick_file_info_read(const char *file_name, Eina_List **groups, Eina_List **images, Eina_List **samples, Eina_List **fonts);

EAPI Eina_Bool edje_pick_group_add(Edje_Pick_Session *session, const char *in_file, const char *in_group);
EAPI Eina_Bool edje_pick_input_file_add(Edje_Pick_Session *session, const char *in_file);
EAPI Eina_Bool edje_pick_output_file_add(Edje_Pick_Session *session, const char *out_file);

EAPI Edje_Pick_Status edje_pick_process(Edje_Pick_Session *session);

EAPI void edje_pick_session_verbose_set(Edje_Pick_Session *session, Eina_Bool verbose);

/* FIXME Daniel: really needed here? parameters order to change (id->name, data, size, ...) */
EAPI Eina_Bool edje_pick_sample_play(void *data, const char *id, size_t size, const double speed, void *finish (void));
#endif

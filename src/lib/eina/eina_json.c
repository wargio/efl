/* EINA - EFL data type library
 * Copyright (C) 2013 Yossi Kantor
 *                    Cedric Bail
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

#include "eina_json.h"
#include <string.h>
#include <math.h>

/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/

#define _RETURN_SW(st, retr) { Stm_Switch _sw; _sw.state = st;\
     _sw.retrans = retr; return _sw; }

#define _CASE_DIGIT case '0':case '1':case '2':\
                    case '3':case '4':case '5':\
                    case '6':case '7':case '8':\
                    case '9'

#define _ISDIGIT(a) (a >= '0' && a <= '9')

#define JSON_GLUE_BUFF_STEP 64

static Eina_Json_Value *_eina_json_type_new(Eina_Json_Type type);

typedef struct _Stm_State Stm_State;
typedef struct _Stm_Machine Stm_Machine;
typedef struct _Stm_switch Stm_Switch;
typedef unsigned int Stm_Val;

typedef Stm_Switch(*Stm_State_Cb)(Stm_State *state, Stm_Val token, void* data);

struct _Stm_switch
{
   Stm_State *state;
   Eina_Bool retrans;
};

struct _Stm_State
{
   Stm_State_Cb state_cb;
   void *param_ptr1;
   int param_int1;
};

struct _Stm_Machine
{
   Stm_State *initial_state;
   Stm_State *current_state;
   void *data;
};

struct _lex_keyword_param
{
   char* str;
   int len;
   Stm_Val token;
};

struct _Eina_Json_Value
{
   EINA_INLIST;
   Eina_Json_Type type;
   Eina_Json_Value *parent;
   union
     {
        double number;
        Eina_Strbuf *string;
        Eina_Bool boolean;
        Eina_Inlist *lst;
        struct _Pair
          {
             Eina_Strbuf *name;
             Eina_Json_Value *val;
          } pair;
     };
};

struct _Eina_Json_Context
{
   Stm_Machine lex_machine;
   Stm_Machine syntax_machine;

   Eina_Json_Error latest_error;

   Eina_Json_Value *jobj;

   Eina_Array *jstack;
   void *jparent;
   Eina_Json_Type jparent_type;
   Eina_Json_Parser_Cb parser_cb;
   void *cb_data;

   const char *str;
   const char *head;

   Eina_Bool glue_on;
   char *glue_buf;
   unsigned glue_buf_size;
   unsigned glue_len;

   unsigned line;
   unsigned col;
};

enum _Eina_Json_Token
{
   JSON_STRING = 1,
   JSON_NUMBER,
   JSON_TRUE,
   JSON_FALSE,
   JSON_NULL,
   JSON_OBJ_OPEN = '{',
   JSON_OBJ_CLOSE = '}',
   JSON_ARR_OPEN  = '[',
   JSON_ARR_CLOSE = ']',
   JSON_COMMA = ',',
   JSON_COLON = ':'
};

// Syntax and Lexical parsing state's callbacks forward declaration
static Stm_Switch _syntx_entry_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _syntx_value_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _syntx_new_object_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _syntx_object_name_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _syntx_object_colon_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _syntx_object_next_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _syntx_array_next_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _syntx_end_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _syntx_unexpected_cb(Stm_State *state, Stm_Val token, void *data);

static Stm_Switch _lex_initial_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _lex_int_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _lex_frac_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _lex_digit_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _lex_exp_sign_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _lex_exp_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _lex_string_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _lex_string_esc_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _lex_keyword_cb(Stm_State *state, Stm_Val token, void *data);
static Stm_Switch _lex_unexpected_cb(Stm_State *state, Stm_Val token, void *data);

// Syntax states
static Stm_State syntx_entry = { _syntx_entry_cb };
static Stm_State syntx_end = { _syntx_end_cb };
static Stm_State syntx_new_object = { _syntx_new_object_cb };
static Stm_State syntx_value = { _syntx_value_cb };
static Stm_State syntx_object_name = { _syntx_object_name_cb };
static Stm_State syntx_object_colon = { _syntx_object_colon_cb };
static Stm_State syntx_next_object =  { _syntx_object_next_cb };
static Stm_State syntx_next_array  = { _syntx_array_next_cb };
static Stm_State syntx_unexpected = { _syntx_unexpected_cb };
static const Stm_Switch switch_syntax_unexpected = { &syntx_unexpected, EINA_TRUE };

// Lexical states
static struct _lex_keyword_param lex_keyword_true = {"true", 4, JSON_TRUE};
static struct _lex_keyword_param lex_keyword_false = {"false", 5, JSON_FALSE};
static struct _lex_keyword_param lex_keyword_null = {"null", 4, JSON_NULL};

static Stm_State lex_initial = {_lex_initial_cb };
static Stm_State lex_int = { _lex_int_cb };
static Stm_State lex_int_entry = {_lex_digit_cb, (void*)&lex_int };
static Stm_State lex_fraction = {_lex_frac_cb };
static Stm_State lex_fraction_entry = {_lex_digit_cb, (void*)&lex_fraction };
static Stm_State lex_exp_sign = { _lex_exp_sign_cb };
static Stm_State lex_exp = { _lex_exp_cb };
static Stm_State lex_exp_entry = {_lex_digit_cb, (void*)&lex_exp };
static Stm_State lex_string = {_lex_string_cb };
static Stm_State lex_string_esc = {_lex_string_esc_cb };
static Stm_State lex_unexpected = { _lex_unexpected_cb };
static Stm_State lex_true = {_lex_keyword_cb, (void*)&lex_keyword_true };
static Stm_State lex_false = {_lex_keyword_cb, (void*)&lex_keyword_false };
static Stm_State lex_null = {_lex_keyword_cb, (void*)&lex_keyword_null };
static const Stm_Switch switch_lex_unexpected = { &lex_unexpected, EINA_TRUE };


void *
_eina_json_parser_dom_cb(Eina_Json_Type type, void *parent, const char *text, void *data)
{
   Eina_Json_Context *jsctx = (Eina_Json_Context *)data;
   Eina_Json_Value *newjval = NULL;

   switch(type)
     {
      case EINA_JSON_TYPE_NULL:
         newjval = eina_json_null_new();
         break;
      case EINA_JSON_TYPE_NUMBER:
         newjval = eina_json_number_new(atof(text));
         break;
      case EINA_JSON_TYPE_STRING:
         newjval = eina_json_string_new(text);
         break;
      case EINA_JSON_TYPE_BOOLEAN:
         newjval = eina_json_boolean_new((*text == 't') ? EINA_TRUE : EINA_FALSE );
         break;
      case EINA_JSON_TYPE_PAIR:
         newjval = _eina_json_type_new(EINA_JSON_TYPE_PAIR);
         if (newjval)
           {
              newjval->pair.name = eina_strbuf_new();
              eina_strbuf_append(newjval->pair.name, text);
           }
         break;
      case EINA_JSON_TYPE_OBJECT:
         newjval = eina_json_object_new();
         break;
      case EINA_JSON_TYPE_ARRAY:
         newjval = eina_json_array_new();
         break;
     }

   if (!newjval) return NULL;

   if (parent)
     {
        Eina_Json_Value *jcontainer = (Eina_Json_Value*)parent;
        switch(jcontainer->type)
          {
           case EINA_JSON_TYPE_PAIR:
              jcontainer->pair.val = newjval;
              break;
           case EINA_JSON_TYPE_OBJECT:
              jcontainer->lst = eina_inlist_append(jcontainer->lst,
                                EINA_INLIST_GET(newjval));
              break;
           case EINA_JSON_TYPE_ARRAY:
              eina_json_array_append(jcontainer, newjval);
              break;
           default:
              break;
          }
     }
   else
     {
        jsctx->jobj = newjval;
     }
   return newjval;
}

void
_state_machine_feed(Stm_Machine *machine, Stm_Val token)
{
   while (machine->current_state)
     {
        Stm_Switch sw =
           machine->current_state->state_cb
           (machine->current_state, token, machine->data);

        machine->current_state = sw.state;
        if (!sw.retrans) break;
     }
}

inline static void
_syntax_token_process(Stm_Val token, Eina_Json_Context *data)
{
   _state_machine_feed(&(data->syntax_machine), token);
}

static Stm_Switch
_syntx_entry_cb(Stm_State *state EINA_UNUSED, Stm_Val token, void *data EINA_UNUSED)
{
   switch (token)
     {
      case JSON_OBJ_OPEN:
      case JSON_ARR_OPEN:
         _RETURN_SW(&syntx_value, EINA_TRUE);
     }
   return switch_syntax_unexpected;
}

static Stm_Switch
_syntx_value_cb(Stm_State *state EINA_UNUSED, Stm_Val token, void *data)
{
   Eina_Json_Context *jsctx = (Eina_Json_Context *)data;
   Stm_State* nextstate = NULL;
   Eina_Json_Type jtype;
   Eina_Bool call_cb = EINA_TRUE;
   void* valptr = NULL;

   switch (token)
     {
      case JSON_OBJ_CLOSE:
      case JSON_ARR_CLOSE:
         if (eina_array_count(jsctx->jstack) < 1)
           _RETURN_SW(&syntx_end, EINA_FALSE);
         jsctx->jparent = eina_array_pop(jsctx->jstack);
         jsctx->jparent_type = (Eina_Json_Type)eina_array_pop(jsctx->jstack);
         call_cb = EINA_FALSE;
         break;

      case JSON_OBJ_OPEN:
         nextstate = &syntx_new_object;
         jtype = EINA_JSON_TYPE_OBJECT;
         break;

      case JSON_ARR_OPEN:
         nextstate = &syntx_value;
         jtype = EINA_JSON_TYPE_ARRAY;
         break;

      case JSON_NUMBER: jtype = EINA_JSON_TYPE_NUMBER; break;
      case JSON_STRING: jtype = EINA_JSON_TYPE_STRING; break;
      case JSON_TRUE: case JSON_FALSE:  jtype = EINA_JSON_TYPE_BOOLEAN; break;
      case JSON_NULL: jtype = EINA_JSON_TYPE_NULL; break;

      default:
        return switch_syntax_unexpected;
     }

   if (call_cb)
     {
        valptr = jsctx->parser_cb(jtype, jsctx->jparent, jsctx->glue_buf,
                                  jsctx->cb_data);

        if (!valptr)
          return switch_syntax_unexpected;
     }

   if (nextstate)
     {
        if (jsctx->jparent)
          {
             eina_array_push(jsctx->jstack, (void*)jsctx->jparent_type);
             eina_array_push(jsctx->jstack, jsctx->jparent);
          }
        jsctx->jparent = valptr;
        jsctx->jparent_type = jtype;
     } else
       {
          if (jsctx->jparent_type == EINA_JSON_TYPE_PAIR)
            {
               jsctx->jparent = eina_array_pop(jsctx->jstack);
               jsctx->jparent_type = (Eina_Json_Type)eina_array_pop(jsctx->jstack);
            }
          nextstate = (jsctx->jparent_type == EINA_JSON_TYPE_OBJECT) ?
                      &syntx_next_object :
                      &syntx_next_array;
       }

     _RETURN_SW(nextstate, EINA_FALSE);
}

static Stm_Switch
_syntx_new_object_cb(Stm_State *state EINA_UNUSED, Stm_Val token, void *data EINA_UNUSED)
{
   switch (token)
     {
      case JSON_OBJ_CLOSE:
         _RETURN_SW(&syntx_value, EINA_TRUE);
      case JSON_STRING:
         _RETURN_SW(&syntx_object_name, EINA_TRUE);
     }
   return switch_syntax_unexpected;
}

static
Stm_Switch _syntx_object_name_cb(Stm_State *state EINA_UNUSED, Stm_Val token, void *data)
{
   Eina_Json_Context *jsctx = (Eina_Json_Context *)data;

   if (token == JSON_STRING)
     {
        eina_array_push(jsctx->jstack, (void*)jsctx->jparent_type);
        eina_array_push(jsctx->jstack, jsctx->jparent);

        jsctx->jparent_type = EINA_JSON_TYPE_PAIR;
        jsctx->jparent = jsctx->parser_cb(EINA_JSON_TYPE_PAIR, jsctx->jparent,
                                          jsctx->glue_buf, jsctx->cb_data);
        if (jsctx->jparent) _RETURN_SW(&syntx_object_colon, EINA_FALSE);
     }
   return switch_syntax_unexpected;
}

static Stm_Switch
_syntx_object_colon_cb(Stm_State *state EINA_UNUSED, Stm_Val token, void *data EINA_UNUSED)
{
   if (token == JSON_COLON)
     _RETURN_SW(&syntx_value, EINA_FALSE);
   return switch_syntax_unexpected;
}

static Stm_Switch
_syntx_object_next_cb(Stm_State *state EINA_UNUSED, Stm_Val token, void *data EINA_UNUSED)
{
   switch (token)
     {
      case JSON_COMMA:
         _RETURN_SW(&syntx_object_name, EINA_FALSE);
      case JSON_OBJ_CLOSE:
         _RETURN_SW(&syntx_value, EINA_TRUE);
     }
   return switch_syntax_unexpected;
}

static Stm_Switch
_syntx_array_next_cb(Stm_State *state EINA_UNUSED, Stm_Val token, void *data EINA_UNUSED)
{
   switch (token)
     {
      case JSON_COMMA:
         _RETURN_SW(&syntx_value, EINA_FALSE);
      case JSON_ARR_CLOSE:
         _RETURN_SW(&syntx_value, EINA_TRUE);
     }
   return switch_syntax_unexpected;
}

static Stm_Switch
_syntx_end_cb(Stm_State *state EINA_UNUSED, Stm_Val token EINA_UNUSED, void *data)
{
   Eina_Json_Context *jsctx = (Eina_Json_Context *)data;
   jsctx->latest_error = EINA_JSON_ERROR_PAST_END;
   _RETURN_SW(NULL, EINA_FALSE);
}

static Stm_Switch
_syntx_unexpected_cb(Stm_State *state EINA_UNUSED, Stm_Val token EINA_UNUSED, void *data)
{
   Eina_Json_Context *jsctx = (Eina_Json_Context *)data;
   if (jsctx->latest_error == EINA_JSON_ERROR_NONE)
     jsctx->latest_error = EINA_JSON_ERROR_SYNTAX_TOKEN;
   _RETURN_SW(NULL, EINA_FALSE);
}

Stm_Switch
_lex_initial_cb(Stm_State *state, Stm_Val token, void *data)
{
   Eina_Json_Context *jsctx = (Eina_Json_Context *)data;

   if (jsctx->glue_on)
     {
        jsctx->glue_buf[0]='\0';
        jsctx->glue_len = 0;
        jsctx->glue_on = EINA_FALSE;
     }

   switch (token)
     {
      case '}': case '{': case ',':
      case ':': case '[': case ']':
         _syntax_token_process(token, data);
         _RETURN_SW (state, EINA_FALSE);

      case '\n':
         jsctx->line ++;
         jsctx->col = 0;
      case '\r': case '\t': case ' ':
         _RETURN_SW (state, EINA_FALSE);

      case '\"':
         _RETURN_SW(&lex_string, EINA_FALSE);

      _CASE_DIGIT:
         _RETURN_SW(&lex_int_entry, EINA_TRUE);
      case '-':
         jsctx->glue_on = EINA_TRUE;
         _RETURN_SW(&lex_int_entry, EINA_FALSE);

      case 't':
         _RETURN_SW(&lex_true, EINA_TRUE);
      case 'f':
         _RETURN_SW(&lex_false, EINA_TRUE);
      case 'n':
         _RETURN_SW(&lex_null, EINA_TRUE);
     }
   return switch_lex_unexpected;
}

static Stm_Switch
_lex_digit_cb(Stm_State *state, Stm_Val token, void *data)
{
   Eina_Json_Context *jsctx = (Eina_Json_Context *)data;
   jsctx->glue_on = EINA_TRUE;

   Stm_State* next_state = (Stm_State*)(state->param_ptr1);
   if (_ISDIGIT(token))
     _RETURN_SW(next_state, EINA_FALSE);

   return switch_lex_unexpected;
}

static Stm_Switch
_lex_int_cb(Stm_State *state, Stm_Val token, void *data)
{
   switch (token)
     {
      _CASE_DIGIT:
        _RETURN_SW(state, EINA_FALSE);
      case 'e':
      case 'E':
        _RETURN_SW(&lex_exp_sign, EINA_FALSE);
      case '.':
        _RETURN_SW(&lex_fraction_entry, EINA_FALSE);
     }
   _syntax_token_process(JSON_NUMBER, data);
   _RETURN_SW(&lex_initial, EINA_TRUE);
}

static Stm_Switch
_lex_frac_cb(Stm_State *state, Stm_Val token, void *data)
{
   switch (token)
     {
      _CASE_DIGIT:
        _RETURN_SW(state, EINA_FALSE);
      case 'e':
      case 'E':
        _RETURN_SW(&lex_exp_sign, EINA_FALSE);
     }
   _syntax_token_process(JSON_NUMBER, data);
   _RETURN_SW (&lex_initial, EINA_TRUE);
}

static Stm_Switch
_lex_exp_sign_cb(Stm_State *state EINA_UNUSED, Stm_Val token, void *data EINA_UNUSED)
{
   switch (token)
     {
      _CASE_DIGIT:
        _RETURN_SW (&lex_exp_entry, EINA_TRUE);
      case '-':
      case '+':
        _RETURN_SW (&lex_exp_entry, EINA_FALSE);
     }
   return switch_lex_unexpected;
}

static Stm_Switch
_lex_exp_cb(Stm_State *state, Stm_Val token, void *data)
{
   switch (token)
     {
      _CASE_DIGIT:
        _RETURN_SW(state, EINA_FALSE);
     }
   _syntax_token_process(JSON_NUMBER, data);
   _RETURN_SW(&lex_initial, EINA_TRUE);
}

static Stm_Switch
_lex_string_cb(Stm_State *state, Stm_Val token, void *data)
{
   Eina_Json_Context *jsctx = (Eina_Json_Context *)data;
   jsctx->glue_on = EINA_TRUE;

   switch (token)
     {
      case '\\':
         _RETURN_SW (&lex_string_esc, EINA_FALSE);
      case '\"':
         _syntax_token_process(JSON_STRING, data);
         _RETURN_SW (&lex_initial, EINA_FALSE);
     }
   _RETURN_SW(state, EINA_FALSE);
}

static Stm_Switch
_lex_string_esc_cb(Stm_State *state EINA_UNUSED, Stm_Val token EINA_UNUSED, void *data EINA_UNUSED)
{
   _RETURN_SW(&lex_string, EINA_FALSE);
}

static Stm_Switch
_lex_keyword_cb(Stm_State *state, Stm_Val token, void *data)
{
   struct _lex_keyword_param *param = state->param_ptr1;

   char* cmpstr = param->str;
   unsigned maxlen = param->len - 1;

   Eina_Json_Context *jsctx = (Eina_Json_Context *)data;

   jsctx->glue_on = EINA_TRUE;

   if ((jsctx->glue_len > maxlen) || (cmpstr[jsctx->glue_len] != (char)token))
     return switch_lex_unexpected;

   if (jsctx->glue_len == maxlen)
     {
        _syntax_token_process(param->token, data);
        _RETURN_SW(&lex_initial, EINA_FALSE);
     }
   _RETURN_SW(state, EINA_FALSE);
}

static Stm_Switch
_lex_unexpected_cb(Stm_State *state EINA_UNUSED, Stm_Val token EINA_UNUSED, void *data)
{
   Eina_Json_Context *jsctx = (Eina_Json_Context *)data;
   if (jsctx->latest_error == EINA_JSON_ERROR_NONE)
     jsctx->latest_error = EINA_JSON_ERROR_LEX_TOKEN;
   _RETURN_SW(NULL, EINA_FALSE);
}

static Eina_Json_Context *
_eina_json_context_new(Eina_Json_Parser_Cb cb, void *data)
{
   Eina_Json_Context *ret = calloc(1, sizeof(Eina_Json_Context));

   if (ret)
     {
        ret->jstack = eina_array_new(10);

        ret->glue_buf_size = JSON_GLUE_BUFF_STEP;
        ret->glue_buf = malloc(ret->glue_buf_size);

        ret->parser_cb = (cb) ? cb : _eina_json_parser_dom_cb;
        ret->cb_data = (cb) ? data : ret;

        if (!(ret->jstack && ret->glue_buf))
          {
             if (ret->jstack) free(ret->jstack);
             if (ret->glue_buf) free(ret->glue_buf);
             free(ret);
             return NULL;
          }

        eina_json_context_reset(ret);
     }
   return ret;
}

static Eina_Bool
_eina_json_context_parse(Eina_Json_Context *ctx, const char *text, unsigned text_len)
{
   unsigned lencount = (text_len) ? text_len : 1;

   ctx->head = ctx->str = text;

   if (ctx->latest_error)
     return EINA_FALSE;

   while (*ctx->head && lencount)
     {
        _state_machine_feed(&(ctx->lex_machine), *ctx->head);

        if (ctx->glue_on)
          {
             if (ctx->glue_buf_size - ctx->glue_len < 2)
               {
                  ctx->glue_buf_size += JSON_GLUE_BUFF_STEP;
                  ctx->glue_buf = realloc(ctx->glue_buf, ctx->glue_buf_size);
               }
             ctx->glue_buf[ctx->glue_len++] = *ctx->head;
             ctx->glue_buf[ctx->glue_len] = '\0';
          }

        if (ctx->latest_error)
          return EINA_FALSE;

        ctx->head++;
        ctx->col++;

        if (text_len) lencount--;
     }
   return EINA_TRUE;
}

static Eina_Json_Value *
_eina_json_parse(const char *text, unsigned size)
{
   Eina_Json_Value *ret = NULL;
   Eina_Json_Context *ctx = eina_json_context_dom_new();
   _eina_json_context_parse(ctx, text, size);
   if (eina_json_context_completed_get(ctx))
     ret = eina_json_context_dom_tree_take(ctx);
   eina_json_context_free(ctx);
   return ret;
}

static Eina_Json_Value *
_eina_json_type_new(Eina_Json_Type type)
{
   Eina_Json_Value *ret = calloc(1, sizeof(Eina_Json_Value));
   if (ret)
      ret->type = type;
   return ret;
}

static unsigned
_eina_json_gen_count_get(Eina_Json_Value *obj)
{
   return eina_inlist_count(obj->lst);
}

static Eina_Json_Value *
_eina_json_gen_nth_get(Eina_Json_Value *obj, unsigned idx)
{
   unsigned count = 0;
   Eina_Inlist *l = NULL;
   for (l = obj->lst; l; l = l->next)
     if ((count++) == idx) break;
   return EINA_INLIST_CONTAINER_GET(l, Eina_Json_Value);
}

static Eina_Json_Value *
_eina_json_gen_append(Eina_Json_Value *obj, Eina_Json_Value *objadd)
{
   if (objadd->parent)
     {
        EINA_LOG_ERR("Can't append json object %p. "
                     "Its already belongs to another json object %p.\n",
                      objadd, objadd->parent);
        return NULL;
     }
   obj->lst = eina_inlist_append(obj->lst, EINA_INLIST_GET(objadd));
   objadd->parent = obj;
   return objadd;
}

static Eina_Json_Value *
_eina_json_gen_insert(Eina_Json_Value *obj, unsigned ind, Eina_Json_Value *jsnval)
{
   Eina_Json_Value *o = _eina_json_gen_nth_get(obj, ind);
   if (!o && ind) return NULL;

   if (jsnval->parent)
     {
        EINA_LOG_ERR("Can't insert json object %p. "
                     "Its already belongs to another json object %p.\n",
                      jsnval, jsnval->parent);
        return NULL;
     }

   obj->lst = eina_inlist_prepend_relative(obj->lst,
                                           EINA_INLIST_GET(jsnval),
                                           EINA_INLIST_GET(o));
   jsnval->parent = obj;
   return jsnval;
}

static Eina_Bool
_eina_json_gen_nth_remove(Eina_Json_Value *obj, unsigned ind)
{
   Eina_Json_Value *o = _eina_json_gen_nth_get(obj, ind);
   if (!o) return EINA_FALSE;
   obj->lst = eina_inlist_remove(obj->lst, EINA_INLIST_GET(o));
   o->parent = NULL;
   eina_json_value_free(o);
   return EINA_TRUE;
}

static inline Eina_Iterator *
_eina_json_gen_iterator_new(Eina_Json_Value *obj)
{
   return eina_inlist_iterator_new(obj->lst);
}

static void
_eina_json_delim_print(Eina_Json_Value *jsnval, Eina_Strbuf *jtext,
                       unsigned ident0, unsigned identV, Eina_Bool objbreak)
{
   int newident;
   Eina_Json_Value *jval;
   Eina_Inlist *l = NULL;

   switch (jsnval->type)
     {
      case EINA_JSON_TYPE_NULL:
         eina_strbuf_append(jtext, "null");
         break;

      case EINA_JSON_TYPE_NUMBER:
         if (ceil(jsnval->number) == jsnval->number)
           eina_strbuf_append_printf(jtext, "%ld", (long)(jsnval->number));
         else
           eina_strbuf_append_printf(jtext, "%.2f", jsnval->number);
         break;

      case EINA_JSON_TYPE_STRING:
         eina_strbuf_append_printf(jtext, "\"%s\"",
                                   eina_strbuf_string_get(jsnval->string));
         break;

      case EINA_JSON_TYPE_BOOLEAN:
         eina_strbuf_append(jtext, (jsnval->boolean) ? "true" : "false");
         break;

      case EINA_JSON_TYPE_PAIR:
         eina_strbuf_append_printf(jtext, "\"%s\"",
                                   eina_strbuf_string_get(jsnval->pair.name));
         eina_strbuf_append(jtext, ":");
         if (objbreak)
           if (jsnval->pair.val->type == EINA_JSON_TYPE_OBJECT)
             eina_strbuf_append_printf(jtext, "\n%*s", ident0, " ");
         _eina_json_delim_print(jsnval->pair.val, jtext, ident0, identV, objbreak);
         break;

      case EINA_JSON_TYPE_ARRAY:
         eina_strbuf_append_char(jtext, '[');

         for (l = jsnval->lst; l; l = l->next)
           {
              jval = EINA_INLIST_CONTAINER_GET(l, Eina_Json_Value);
              _eina_json_delim_print(jval, jtext, ident0, identV, objbreak);
              if (l->next) eina_strbuf_append(jtext, (objbreak) ? ", " : ",");
           }
         eina_strbuf_append_char(jtext, ']');
         break;

      case EINA_JSON_TYPE_OBJECT:
         if (!jsnval->lst)
           {
              eina_strbuf_append(jtext, "{}");
              break;
           }
         newident = ident0 + identV;
         eina_strbuf_append_char(jtext, '{');

         for (l = jsnval->lst; l;l = l->next)
           {
              if (objbreak)
                eina_strbuf_append_printf(jtext, "\n%*s", newident, " ");
              jval = EINA_INLIST_CONTAINER_GET(l, Eina_Json_Value);
              _eina_json_delim_print(jval, jtext, newident, identV, objbreak);
              if (l->next) eina_strbuf_append(jtext, ",");
           }
         if (objbreak)
           eina_strbuf_append_printf(jtext, "\n%*s", ident0, " ");

         eina_strbuf_append_char(jtext, '}');
         break;
     }
}

/*============================================================================*
 *                          JSON Parser API                                   *
 *============================================================================*/

EAPI void
eina_json_context_reset(Eina_Json_Context* ctx)
{
   Stm_Machine * lexm = &(ctx->lex_machine);
   Stm_Machine * stxm = &(ctx->syntax_machine);

   lexm->current_state = lexm->initial_state = &lex_initial;
   stxm->current_state = stxm->initial_state = &syntx_entry;
   stxm->data = lexm->data = ctx;

   ctx->latest_error = EINA_JSON_ERROR_NONE;

   ctx->glue_len = 0;
   ctx->glue_buf[0] = '\0';
   ctx->glue_on = EINA_FALSE;

   eina_array_clean(ctx->jstack);
   eina_json_value_free(ctx->jobj);
   ctx->jobj = NULL;
   ctx->jparent=NULL;

   ctx->str = ctx->head = NULL;
   ctx->line = ctx->col = 1;
}

EAPI Eina_Json_Context *
eina_json_context_dom_new()
{
   return _eina_json_context_new(NULL, NULL);
}

EAPI Eina_Json_Context *
eina_json_context_sax_new(Eina_Json_Parser_Cb cb, void* data)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cb, NULL);
   return _eina_json_context_new(cb, data);
}

EAPI void
eina_json_context_free(Eina_Json_Context* ctx)
{
   if (!ctx) return;

   eina_json_context_reset(ctx);
   free(ctx->glue_buf);
   eina_array_free(ctx->jstack);
   free(ctx);
}

EAPI inline unsigned
eina_json_context_line_get(Eina_Json_Context* ctx)
{
   return ctx->line;
}

EAPI inline unsigned
eina_json_context_column_get(Eina_Json_Context* ctx)
{
   return ctx->col;
}

EAPI inline Eina_Json_Error
eina_json_context_error_get(Eina_Json_Context* ctx)
{
   return ctx->latest_error;
}

EAPI inline Eina_Bool
eina_json_context_completed_get(Eina_Json_Context* ctx)
{
   return (ctx->syntax_machine.current_state == &syntx_end);
}

EAPI Eina_Bool
eina_json_context_unfinished_get(Eina_Json_Context* ctx)
{
   return (Eina_Bool)(!eina_json_context_completed_get(ctx) &&
                      !eina_json_context_error_get(ctx));
}

EAPI Eina_Json_Value *
eina_json_context_dom_tree_take(Eina_Json_Context* ctx)
{
   if (!eina_json_context_completed_get(ctx))
     {
        EINA_LOG_ERR("Taking json tree from erroneous or uncompleted json context");
        return NULL;
     }
   if (!ctx->jobj)
     {
        EINA_LOG_ERR("Json tree already taken for this json context");
        return NULL;
     }
   Eina_Json_Value* ret = ctx->jobj;
   ctx->jobj = NULL;
   return ret;
}

EAPI inline Eina_Json_Value *
eina_json_parse(const char *text)
{
   return _eina_json_parse(text, 0);
}

EAPI Eina_Json_Value *
eina_json_parse_n(const char *text, unsigned text_len)
{
   if (!text_len) return NULL;
   return _eina_json_parse(text, text_len);
}

EAPI Eina_Bool
eina_json_context_parse(Eina_Json_Context *ctx, const char *text)
{
   return _eina_json_context_parse(ctx, text, 0);
}

EAPI Eina_Bool
eina_json_context_parse_n(Eina_Json_Context *ctx, const char *text, unsigned text_len)
{
   if (!text_len) return !!(ctx->latest_error);
   return _eina_json_context_parse(ctx, text, text_len);
}

/*============================================================================*
 *                          JSON Value manipulation API                       *
 *============================================================================*/

EAPI Eina_Json_Value *
eina_json_number_new(double num)
{
   Eina_Json_Value *ret = _eina_json_type_new(EINA_JSON_TYPE_NUMBER);
   if (ret) ret->number = num;
   return ret;
}

EAPI Eina_Json_Value *
eina_json_string_new(const char* string)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(string, NULL);
   Eina_Json_Value *ret =  _eina_json_type_new(EINA_JSON_TYPE_STRING);
   if (ret)
     {
        ret->string = eina_strbuf_new();
        eina_strbuf_append(ret->string, string);
     }
   return ret;
}

EAPI Eina_Json_Value *
eina_json_boolean_new(Eina_Bool boolval)
{
   Eina_Json_Value *ret = _eina_json_type_new(EINA_JSON_TYPE_BOOLEAN);
   if (ret) ret->boolean = boolval;
   return ret;
}

EAPI inline Eina_Json_Value *
eina_json_null_new()
{
   return _eina_json_type_new(EINA_JSON_TYPE_NULL);
}

EAPI Eina_Json_Value *
eina_json_object_new()
{
   return _eina_json_type_new(EINA_JSON_TYPE_OBJECT);
}

EAPI Eina_Json_Value *
eina_json_array_new()
{
   return _eina_json_type_new(EINA_JSON_TYPE_ARRAY);
}

EAPI void
eina_json_value_free(Eina_Json_Value *jsnval)
{
   if (!jsnval) return;

   if (jsnval->parent)
     {
        EINA_LOG_ERR("Can't free json object %p. It belongs to %p.\n",
                     jsnval, jsnval->parent);
        return;
     }

   switch (jsnval->type)
     {
      case EINA_JSON_TYPE_STRING:
         eina_strbuf_free(jsnval->string);
         break;
      case EINA_JSON_TYPE_PAIR:
         eina_strbuf_free(jsnval->pair.name);
         if (jsnval->pair.val) jsnval->pair.val->parent = NULL;
         eina_json_value_free(jsnval->pair.val);
         break;
      case EINA_JSON_TYPE_OBJECT:
      case EINA_JSON_TYPE_ARRAY:
         while (jsnval->lst)
           {
              Eina_Json_Value *aux = EINA_INLIST_CONTAINER_GET(jsnval->lst,
                                                               Eina_Json_Value);
              jsnval->lst = eina_inlist_remove(jsnval->lst, jsnval->lst);
              aux->parent = NULL;
              eina_json_value_free(aux);
           }
      default:
         break;
     }
   free(jsnval);
}

EAPI inline
Eina_Json_Type eina_json_type_get(Eina_Json_Value *jsnval)
{
   return jsnval->type;
}

EAPI Eina_Bool
eina_json_number_set(Eina_Json_Value *jsnval, double num)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((jsnval->type != EINA_JSON_TYPE_NUMBER), EINA_FALSE);
   jsnval->number = num;
   return EINA_TRUE;
}

EAPI Eina_Bool
eina_json_string_set(Eina_Json_Value *jsnval, const char* string)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((jsnval->type != EINA_JSON_TYPE_STRING), EINA_FALSE);
   eina_strbuf_reset(jsnval->string);
   eina_strbuf_append(jsnval->string, string);
   return EINA_TRUE;
}

EAPI Eina_Bool
eina_json_boolean_set(Eina_Json_Value *jsnval, Eina_Bool boolval)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((jsnval->type != EINA_JSON_TYPE_BOOLEAN), EINA_FALSE);
   jsnval->boolean = boolval;
   return EINA_TRUE;
}

EAPI double
eina_json_number_get(Eina_Json_Value *jsnval)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((jsnval->type != EINA_JSON_TYPE_NUMBER), 0.0);
   return jsnval->number;
}

EAPI const char *
eina_json_string_get(Eina_Json_Value *jsnval)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((jsnval->type != EINA_JSON_TYPE_STRING), NULL);
   return eina_strbuf_string_get(jsnval->string);
}

EAPI Eina_Bool
eina_json_boolean_get(Eina_Json_Value *jsnval)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((jsnval->type != EINA_JSON_TYPE_BOOLEAN), EINA_FALSE);
   return jsnval->boolean;
}

EAPI const char *
eina_json_pair_name_get(Eina_Json_Value *obj)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((obj->type != EINA_JSON_TYPE_PAIR), NULL);
   return eina_strbuf_string_get(obj->pair.name);
}

EAPI Eina_Json_Value *
eina_json_pair_value_get(Eina_Json_Value *obj)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((obj->type != EINA_JSON_TYPE_PAIR), NULL);
   return obj->pair.val;
}

EAPI unsigned
eina_json_object_count_get(Eina_Json_Value *obj)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((obj->type != EINA_JSON_TYPE_OBJECT), 0);
   return _eina_json_gen_count_get(obj);
}

EAPI Eina_Json_Value *
eina_json_object_nth_get(Eina_Json_Value *obj, unsigned idx)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((obj->type != EINA_JSON_TYPE_OBJECT), NULL);
   return _eina_json_gen_nth_get(obj, idx);
}

EAPI Eina_Json_Value *
eina_json_object_append(Eina_Json_Value *obj, const char* keyname, Eina_Json_Value *jsnval)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((obj->type != EINA_JSON_TYPE_OBJECT), NULL);
   EINA_SAFETY_ON_TRUE_RETURN_VAL((jsnval->type == EINA_JSON_TYPE_PAIR), NULL);

   if (jsnval->parent)
     {
        EINA_LOG_ERR("Can't append json object %p.Its already "
                     "belongs to another json object %p.\n",
                      jsnval, jsnval->parent);
        return NULL;
     }

   Eina_Json_Value *pr = _eina_json_type_new(EINA_JSON_TYPE_PAIR);
   pr->pair.name = eina_strbuf_new();
   eina_strbuf_append(pr->pair.name, keyname);
   pr->pair.val = jsnval;

   Eina_Json_Value *ins = _eina_json_gen_append(obj, pr);
   return (ins) ? pr : NULL;
}

EAPI Eina_Json_Value *
eina_json_object_insert(Eina_Json_Value *obj, unsigned idx, const char* keyname, Eina_Json_Value *jsnval)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((obj->type != EINA_JSON_TYPE_OBJECT), NULL);
   EINA_SAFETY_ON_TRUE_RETURN_VAL((jsnval->type == EINA_JSON_TYPE_PAIR), NULL);

   Eina_Json_Value *o = _eina_json_gen_nth_get(obj, idx);
   if (!o && idx) return NULL;

   if (jsnval->parent)
     {
        EINA_LOG_ERR("Can't insert json object %p.Its already "
                     "belongs to another json object %p.\n",
                      jsnval, jsnval->parent);
        return NULL;
     }

   Eina_Json_Value *pr = _eina_json_type_new(EINA_JSON_TYPE_PAIR);
   pr->pair.name = eina_strbuf_new();
   eina_strbuf_append(pr->pair.name, keyname);
   pr->pair.val = jsnval;
   jsnval->parent = pr;

   Eina_Json_Value *ins = _eina_json_gen_insert(obj, idx, pr);
   return (ins) ? pr : NULL;
}

EAPI Eina_Bool
eina_json_object_nth_remove(Eina_Json_Value *obj, unsigned idx)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((obj->type != EINA_JSON_TYPE_OBJECT), EINA_FALSE);
   return _eina_json_gen_nth_remove(obj, idx);
}

EAPI Eina_Iterator *
eina_json_object_iterator_new(Eina_Json_Value *obj)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((obj->type != EINA_JSON_TYPE_OBJECT), NULL);
   return _eina_json_gen_iterator_new(obj);
}

EAPI Eina_Json_Value *
eina_json_object_value_get_internal(Eina_Json_Value *obj, ...)
{
   char* jkey;
   va_list vl;
   Eina_Json_Value *jval;
   Eina_Inlist *l;

   EINA_SAFETY_ON_TRUE_RETURN_VAL((obj->type != EINA_JSON_TYPE_OBJECT), NULL);

   jval = obj;
   va_start(vl, obj);
   while ((jkey = va_arg(vl, char*)))
     {
        if (jval->type != EINA_JSON_TYPE_OBJECT) return NULL;
        for (l = jval->lst; l; l = l->next)
          {
             jval = EINA_INLIST_CONTAINER_GET(l, Eina_Json_Value);
             if (!strcmp(eina_strbuf_string_get(jval->pair.name), jkey)) break;
             jval = NULL;
          }
        if (!jval) break;
        jval = jval->pair.val;
     }
   va_end(vl);

   if (jval == obj) jval = NULL;

   return jval;
}

EAPI unsigned
eina_json_array_count_get(Eina_Json_Value *arr)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((arr->type != EINA_JSON_TYPE_ARRAY), 0);
   return _eina_json_gen_count_get(arr);
}

EAPI Eina_Json_Value *
eina_json_array_nth_get(Eina_Json_Value *arr, unsigned idx)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((arr->type != EINA_JSON_TYPE_ARRAY), NULL);
   return _eina_json_gen_nth_get(arr, idx);
}

EAPI Eina_Json_Value *
eina_json_array_append(Eina_Json_Value *arr, Eina_Json_Value *jsnval)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((arr->type != EINA_JSON_TYPE_ARRAY), NULL);
   EINA_SAFETY_ON_TRUE_RETURN_VAL((jsnval->type == EINA_JSON_TYPE_PAIR), NULL);
   return _eina_json_gen_append(arr, jsnval);
}

EAPI Eina_Json_Value *
eina_json_array_insert(Eina_Json_Value *arr, unsigned idx, Eina_Json_Value *jsnval)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((arr->type != EINA_JSON_TYPE_ARRAY), NULL);
   EINA_SAFETY_ON_TRUE_RETURN_VAL((jsnval->type == EINA_JSON_TYPE_PAIR), NULL);
   return _eina_json_gen_insert(arr, idx, jsnval);
}

EAPI Eina_Bool
eina_json_array_nth_remove(Eina_Json_Value *arr, unsigned idx)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((arr->type != EINA_JSON_TYPE_ARRAY), EINA_FALSE);
   return _eina_json_gen_nth_remove(arr, idx);
}

EAPI Eina_Iterator *
eina_json_array_iterator_new(Eina_Json_Value *arr)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((arr->type != EINA_JSON_TYPE_ARRAY), NULL);
   return _eina_json_gen_iterator_new(arr);
}

EAPI char *
eina_json_format_string_get(Eina_Json_Value *jsnval, Eina_Json_Format format)
{
   EINA_SAFETY_ON_TRUE_RETURN_VAL((format > EINA_JSON_FORMAT_BASIC), NULL);

   int ident0 = 0;
   int identV = (format) ? 2 : 0;
   int objbreak = (format) ? EINA_TRUE : EINA_FALSE;

   Eina_Strbuf *jtext = eina_strbuf_new();
   _eina_json_delim_print(jsnval, jtext, ident0, identV, objbreak);
   char *ret = eina_strbuf_string_steal(jtext);
   eina_strbuf_free(jtext);
   return ret;
}

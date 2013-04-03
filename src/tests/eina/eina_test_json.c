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

#include <stdio.h>

#include "eina_suite.h"
#include "Eina.h"

typedef struct
{
   unsigned line;
   unsigned col;
} TextPos;

typedef struct
{
   Eina_Strbuf *text;
   unsigned long parent_idx;
} Sax_Parser_Data;

static const  char *json_type_string[] = {"NL","NR","STR","BN","PAR","OBJ","ARR"};

static const char jstr_array_before[] =
"{\
\"Array1\":[0,1,2,3,4,5,6],\n\
\"Array2\":[56,\"He\",true,\"Hello\",false,null],\n\
\"Array3\":[\"\",\"He\",true,\"Hello\",false,null],\n\
\"Array4\":[]\
}";

static const char jstr_array_after[] =
"{\"Array1\":[0,1,2,3,4,5,6],\"Array2\":[57,56,156,\"new\",\
false,\"Bye\",true,null],\"Array3\":[],\"Array4\":[1]}";

static const char jstr_object_before[] =
"{\
\"Object1\":{\"0\":0,\"1\":1,\"2\":2,\"3\":3,\"4\":4,\"5\":5,\"6\":6},\n\
\"Object2\":{\"Num1\":56,\"Str1\":\"Str1\",\"Bool1\":true,\"Str2\":\"Hello\",\"Null\":null},\n\
\"Object3\":{\"Num1\":56,\"Bool1\":true,\"String1\":\"String\",\"Null\":null},\
\"Object4\":{}\
}";

static const char jstr_object_after[] =
"{\"Object1\":{\"0\":0,\"1\":1,\"2\":2,\"3\":3,\"4\":4,\"5\":5,\"6\":6},\
\"Object2\":{\"Num3\":57,\"Num2\":56,\"Num1\":156,\"Str3\":\"new\",\"Bool1\":false,\
\"Str2\":\"HelloStr2\",\"Null\":null},\"Object3\":{},\"Object4\":{\"NumberOne\":1},\
\"Object5\":true}";

static const char jstr_object_tree[] =
"{ \"Obj1\":{ \"Obj1_1\":11, \"Obj1_2\":12 },\"Obj2\":2 }";

static const TextPos pos_full = {32,2};
static const char jstr_full[] =
"{\n\
  \"Type1\":\"John\",\n\
  \"Type2\":\"Smith\",\n\
  \"Type3\":25,\n\
  \"Type4\":null,\n\
  \"Type5\":true,\n\
  \"Type6\":false,\n\
  \"Type7\":\n\
  {\n\
    \"Type1\":\"John\",\n\
    \"Type2\":\"Smith\",\n\
    \"Type3\":25,\n\
    \"Type4\":null,\n\
    \"Type5\":true,\n\
    \"Type6\":false\n\
  },\n\
  \"Type8\":[\"John\",\"Smith\",\" Escaped \\\" \",25,null,true,false],\n\
  \"TypeNum\":[0,-1,1,2.0,3.45,-4.67e2,5e-1,6e3,5e+1,-5.6e+2],\n\
  \"TypeMix\":\n\
  [\n\
    67,null,[],{\"Hello\":[true]},false,\"Bye\",\n\
    {\n\
      \"Type21\":\"John\",\n\
      \"Type22\":\"Smith\",\n\
      \"Type23\":{}\n\
    },\n\
    {\n\
      \"type\":\"fax\",\n\
      \"number\":\"646 555-4567\"\n\
    }\n\
  ]\n\
}";

static const char jstr_double_root[] =
"{\n\
  \"Type1\":\"John\",\n\
  \"Type2\":\"Smith\",\n\
  \"Type3\":25,\n\
  \"Type4\":null,\n\
  \"Type5\":true,\n\
  \"Type6\":false,\n\
  \"Type7\":\n\
  {\n\
    \"Type1\":\"John\",\n\
    \"Type2\":\"Smith\",\n\
    \"Type3\":25,\n\
    \"Type4\":null,\n\
    \"Type5\":true,\n\
    \"Type6\":false\n\
  },\n\
  \"Type8\":[\"John\",\"Smith\",\" Escaped \\\" \",25,null,true,false],\n\
  \"TypeNum\":[0,-1,1,2.0,3.45,-4.67e2,5e-1,6e3,5e+1,-5.6e+2],\n\
  \"TypeMix\":\n\
  [\n\
    67,null,[],{\"Hello\":[true]},false,\"Bye\",\n\
    {\n\
      \"Type21\":\"John\",\n\
      \"Type22\":\"Smith\",\n\
      \"Type23\":{}\n\
    },\n\
    {\n\
      \"type\":\"fax\",\n\
      \"number\":\"646 555-4567\"\n\
    }\n\
  ]\n\
}             \n\
{ \"DoubleRoot\":null }\n\
";

static const TextPos pos_lex_error = {4,13};
char jstr_lex_error[] =
"{\n\
  \"Type1\":\"John\",\n\
  \"Type2\":\"Smith\",\n\
  \"Type3\":25a,\n\
  \"Type4\":null,\n\
  \"Type5\":true,\n\
}";

static const TextPos pos_syntax_error = {3,17};
char jstr_syntax_error[] =
"{\n\
  \"Type1\":\"John\",\n\
  \"Type2\" \"Smith\",\n\
  \"Type3\":25,\n\
  \"Type4\":null,\n\
  \"Type5\":true,\n\
}";

static const TextPos pos_incomplete = {6,15};
char jstr_incomplete[]=
"{\n\
  \"Type1\":\"John\",\n\
  \"Type2\":\"Smith\",\n\
  \"Type3\":25,\n\
  \"Type4\":null,\n\
  \"Type5\":true";


static const char jstr_full_packed[] =
"\
{\"Type1\":\"John\",\"Type2\":\"Smith\",\"Type3\":25,\"Type4\":null,\
\"Type5\":true,\"Type6\":false,\"Type7\":{\"Type1\":\"John\",\
\"Type2\":\"Smith\",\"Type3\":25,\"Type4\":null,\"Type5\":true,\
\"Type6\":false},\"Type8\":[\"John\",\"Smith\",\
\" Escaped \\\" \",25,null,true,false],\
\"TypeNum\":[0,-1,1,2,3.45,-467,0.50,6000,50,-560],\
\"TypeMix\":[67,null,[],{\"Hello\":[true]},false,\"Bye\",\
{\"Type21\":\"John\",\"Type22\":\"Smith\",\"Type23\":{}},\
{\"type\":\"fax\",\"number\":\"646 555-4567\"}]}\
";

static const char jstr_sax_result[] =
"\
(0x2)PR((nil)):OBJ(0x3)PR(0x2):PAR(\"Type1\")(0x4)PR(0x3):STR(\"John\")\
(0x5)PR(0x2):PAR(\"Type2\")(0x6)PR(0x5):STR(\"Smith\")(0x7)PR(0x2):\
PAR(\"Type3\")(0x8)PR(0x7):NR(\"25\")(0x9)PR(0x2):PAR(\"Type4\")(0xa)PR\
(0x9):NL(0xb)PR(0x2):PAR(\"Type5\")(0xc)PR(0xb):BN(\"true\")(0xd)PR(0x2):\
PAR(\"Type6\")(0xe)PR(0xd):BN(\"false\")(0xf)PR(0x2):PAR(\"Type7\")\
(0x10)PR(0xf):OBJ(0x11)PR(0x10):PAR(\"Type1\")(0x12)PR(0x11):\
STR(\"John\")(0x13)PR(0x10):PAR(\"Type2\")(0x14)PR(0x13):STR(\"Smith\")\
(0x15)PR(0x10):PAR(\"Type3\")(0x16)PR(0x15):NR(\"25\")(0x17)PR(0x10):\
PAR(\"Type4\")(0x18)PR(0x17):NL(0x19)PR(0x10):PAR(\"Type5\")(0x1a)PR(0x19)\
:BN(\"true\")(0x1b)PR(0x10):PAR(\"Type6\")(0x1c)PR(0x1b):BN(\"false\")\
(0x1d)PR(0x2):PAR(\"Type8\")(0x1e)PR(0x1d):ARR(0x1f)PR(0x1e):STR(\"John\")\
(0x20)PR(0x1e):STR(\"Smith\")(0x21)PR(0x1e):STR(\" Escaped \\\" \")\
(0x22)PR(0x1e):NR(\"25\")(0x23)PR(0x1e):NL(0x24)PR(0x1e):BN(\"true\")\
(0x25)PR(0x1e):BN(\"false\")(0x26)PR(0x2):PAR(\"TypeNum\")(0x27)PR(0x26)\
:ARR(0x28)PR(0x27):NR(\"0\")(0x29)PR(0x27):NR(\"-1\")(0x2a)PR(0x27):\
NR(\"1\")(0x2b)PR(0x27):NR(\"2.0\")(0x2c)PR(0x27):NR(\"3.45\")(0x2d)\
PR(0x27):NR(\"-4.67e2\")(0x2e)PR(0x27):NR(\"5e-1\")(0x2f)PR(0x27):\
NR(\"6e3\")(0x30)PR(0x27):NR(\"5e+1\")(0x31)PR(0x27):NR(\"-5.6e+2\")\
(0x32)PR(0x2):PAR(\"TypeMix\")(0x33)PR(0x32):ARR(0x34)PR(0x33):\
NR(\"67\")(0x35)PR(0x33):NL(0x36)PR(0x33):ARR(0x37)PR(0x33):OBJ(0x38)\
PR(0x37):PAR(\"Hello\")(0x39)PR(0x38):ARR(0x3a)PR(0x39):BN(\"true\")\
(0x3b)PR(0x33):BN(\"false\")(0x3c)PR(0x33):STR(\"Bye\")(0x3d)PR(0x33):\
OBJ(0x3e)PR(0x3d):PAR(\"Type21\")(0x3f)PR(0x3e):STR(\"John\")(0x40)\
PR(0x3d):PAR(\"Type22\")(0x41)PR(0x40):STR(\"Smith\")(0x42)PR(0x3d)\
:PAR(\"Type23\")(0x43)PR(0x42):OBJ(0x44)PR(0x33):OBJ(0x45)PR(0x44)\
:PAR(\"type\")(0x46)PR(0x45):STR(\"fax\")(0x47)PR(0x44):PAR(\"number\")\
(0x48)PR(0x47):STR(\"646 555-4567\")\
";

static void *
sax_parser_cb(Eina_Json_Type type, void *parent, const char *text, void *data)
{
   Sax_Parser_Data *saxdata = (Sax_Parser_Data *)data;
   const char* strval = NULL;

   switch(type)
     {
      case EINA_JSON_TYPE_NUMBER:
      case EINA_JSON_TYPE_STRING:
      case EINA_JSON_TYPE_PAIR:
         strval = text;
         break;
      case EINA_JSON_TYPE_BOOLEAN:
         strval = (*text == 't') ? "true" : "false";
         break;
      default:
         break;
     }

   saxdata->parent_idx++;
   eina_strbuf_append_printf(saxdata->text,
                             "(%p)PR(%p):%s",
                             (void*)saxdata->parent_idx,
                             parent,
                             json_type_string[type]);

   if (strval) eina_strbuf_append_printf(saxdata->text, "(\"%s\")", strval);

   return (void*)saxdata->parent_idx;
}

START_TEST(eina_json_parse_test)
{
   eina_init();

   Eina_Json_Value* jval = eina_json_parse(jstr_full);
   fail_unless(jval != NULL);
   char* jval_str = eina_json_format_string_get(jval, EINA_JSON_FORMAT_PACKED);
   fail_if(strcmp(jstr_full_packed, jval_str));
   free(jval_str);
   eina_json_value_free(jval);

   fail_if(eina_json_parse(jstr_lex_error));
   fail_if(eina_json_parse(jstr_syntax_error));
   fail_if(eina_json_parse(jstr_incomplete));
   fail_if(eina_json_parse(jstr_double_root));

   int json_full_len = strlen(jstr_full);

   jval = eina_json_parse_n(jstr_double_root, json_full_len);
   fail_unless(jval != NULL);
   jval_str = eina_json_format_string_get(jval, EINA_JSON_FORMAT_PACKED);
   fail_if(strcmp(jstr_full_packed, jval_str));
   free(jval_str);
   eina_json_value_free(jval);

   jval = eina_json_parse_n(jstr_full, 0xFFFF);
   fail_unless(jval != NULL);
   jval_str = eina_json_format_string_get(jval, EINA_JSON_FORMAT_PACKED);
   fail_if(strcmp(jstr_full_packed, jval_str));
   free(jval_str);
   eina_json_value_free(jval);

   fail_if(eina_json_parse_n(jstr_full, json_full_len / 2));
   fail_if(eina_json_parse_n(jstr_full, 0));

   eina_shutdown();
}
END_TEST

START_TEST(eina_json_context_dom_parse_test)
{
   eina_init();

   Eina_Bool res;
   Eina_Json_Context *ctx = eina_json_context_dom_new();
   fail_if(eina_json_context_dom_tree_take(ctx));
   res = eina_json_context_parse(ctx, jstr_full);
   fail_unless(res);
   fail_unless(eina_json_context_completed_get(ctx));
   fail_if(eina_json_context_unfinished_get(ctx));
   fail_if(eina_json_context_line_get(ctx) != pos_full.line);
   fail_if(eina_json_context_column_get(ctx) != pos_full.col);
   fail_unless(eina_json_context_error_get(ctx) == EINA_JSON_ERROR_NONE);

   res = eina_json_context_parse(ctx, "{");
   fail_if(res);
   fail_if(eina_json_context_completed_get(ctx));
   fail_if(eina_json_context_dom_tree_take(ctx));
   fail_if(eina_json_context_unfinished_get(ctx));
   fail_if(eina_json_context_line_get(ctx) != pos_full.line);
   fail_if(eina_json_context_column_get(ctx) != pos_full.col);
   fail_unless(eina_json_context_error_get(ctx) == EINA_JSON_ERROR_PAST_END);

   eina_json_context_reset(ctx);

   res = eina_json_context_parse(ctx, jstr_incomplete);
   fail_unless(res);
   fail_if(eina_json_context_completed_get(ctx));
   fail_if(eina_json_context_line_get(ctx) != pos_incomplete.line);
   fail_if(eina_json_context_column_get(ctx) != pos_incomplete.col);
   fail_unless(eina_json_context_error_get(ctx) == EINA_JSON_ERROR_NONE);

   res = eina_json_context_parse(ctx, "}");
   fail_unless(res);
   fail_unless(eina_json_context_completed_get(ctx));
   fail_if(eina_json_context_line_get(ctx) != pos_incomplete.line);
   fail_if(eina_json_context_column_get(ctx) != pos_incomplete.col+1);
   fail_unless(eina_json_context_error_get(ctx) == EINA_JSON_ERROR_NONE);

   eina_json_context_reset(ctx);

   res = eina_json_context_parse(ctx, jstr_lex_error);
   fail_if(res);
   fail_if(eina_json_context_completed_get(ctx));
   fail_if(eina_json_context_line_get(ctx) != pos_lex_error.line);
   fail_if(eina_json_context_column_get(ctx) != pos_lex_error.col);
   fail_unless(eina_json_context_error_get(ctx) == EINA_JSON_ERROR_LEX_TOKEN);

   eina_json_context_reset(ctx);

   res = eina_json_context_parse(ctx, jstr_syntax_error);
   fail_if(res);
   fail_if(eina_json_context_completed_get(ctx));
   fail_if(eina_json_context_line_get(ctx) != pos_syntax_error.line);
   fail_if(eina_json_context_column_get(ctx) != pos_syntax_error.col);
   fail_unless(eina_json_context_error_get(ctx) == EINA_JSON_ERROR_SYNTAX_TOKEN);

   eina_json_context_reset(ctx);

   int len = strlen(jstr_double_root);
   const int bsize = 3;
   int head = 0;

   while (eina_json_context_unfinished_get(ctx))
     {
        eina_json_context_parse_n(ctx, &jstr_double_root[head], bsize);
        head += bsize;
        fail_if(len-head < bsize);
     }

   Eina_Json_Value *jval = eina_json_context_dom_tree_take(ctx);
   fail_unless(jval != NULL);
   fail_if(eina_json_context_dom_tree_take(ctx));
   char *jval_str = eina_json_format_string_get(jval, EINA_JSON_FORMAT_PACKED);
   fail_if(strcmp(jstr_full_packed, jval_str));
   free(jval_str);
   eina_json_value_free(jval);

   eina_json_context_free(ctx);
   eina_shutdown();
}
END_TEST

START_TEST(eina_json_context_sax_parse_test)
{
   eina_init();

   Sax_Parser_Data testsax;

   testsax.text = eina_strbuf_new();
   testsax.parent_idx = 1;

   Eina_Json_Context *ctx = eina_json_context_sax_new(sax_parser_cb, &testsax);

   Eina_Bool res = eina_json_context_parse(ctx, jstr_full);
   fail_unless(res);
   fail_unless(eina_json_context_completed_get(ctx));
   fail_if(eina_json_context_dom_tree_take(ctx));

   fail_if(strcmp(eina_strbuf_string_get(testsax.text), jstr_sax_result));

   eina_strbuf_free(testsax.text);
   eina_json_context_free(ctx);
   eina_shutdown();
}
END_TEST

START_TEST(eina_json_object_test)
{
   eina_init();

   Eina_Json_Value * jobj = eina_json_parse(jstr_object_before);
   fail_if(!jobj);

   Eina_Json_Value * tmp;

   Eina_Json_Value *obj1 = eina_json_pair_value_get(eina_json_object_nth_get(jobj, 0));
   Eina_Json_Value *obj2 = eina_json_pair_value_get(eina_json_object_nth_get(jobj, 1));
   Eina_Json_Value *obj3 = eina_json_pair_value_get(eina_json_object_nth_get(jobj, 2));
   Eina_Json_Value *obj4 = eina_json_pair_value_get(eina_json_object_nth_get(jobj, 3));

   fail_if(eina_json_object_nth_get(obj4, 0));

   tmp = eina_json_number_new(1);
   fail_if(eina_json_object_insert(obj4, 1, "tmp", tmp));
   eina_json_value_free(tmp);

   fail_unless(eina_json_object_insert(obj4, 0, "NumberOne",
                                       eina_json_number_new(1)) != NULL);

   fail_unless(eina_json_object_nth_get(obj4, 0)  != NULL );

   tmp = eina_json_number_new(1);
   fail_if(eina_json_object_insert(obj4, 1, "tmp", tmp));
   eina_json_value_free(tmp);

   fail_if(eina_json_object_nth_get(obj3, 11));
   fail_if(eina_json_object_nth_remove(obj3, 11));
   fail_if(eina_json_object_nth_remove(obj3, 7));
   fail_if(eina_json_object_count_get(obj3) != 4);
   while(eina_json_object_count_get(obj3))
     fail_unless(eina_json_object_nth_remove(obj3, 0));

   fail_if(eina_json_object_nth_remove(obj3, 0));
   fail_if(eina_json_object_nth_get(obj3, 0));

   tmp = eina_json_pair_value_get(eina_json_object_nth_get(obj2, 4));
   fail_unless(eina_json_type_get(tmp) == EINA_JSON_TYPE_NULL);

   fail_unless(eina_json_object_nth_remove(obj2, 1));

   tmp = eina_json_pair_value_get(eina_json_object_nth_get(obj2, 3));
   fail_unless(eina_json_type_get(tmp) == EINA_JSON_TYPE_NULL);

   tmp = eina_json_pair_value_get(eina_json_object_nth_get(obj2, 1));
   fail_unless(eina_json_boolean_set(tmp, !eina_json_boolean_get(tmp)));

   tmp = eina_json_pair_value_get(eina_json_object_nth_get(obj2, 0));
   fail_unless(eina_json_number_set(tmp, eina_json_number_get(tmp) + 100));

   tmp = eina_json_object_nth_get(obj2, 2);
   Eina_Strbuf *joinstr = eina_strbuf_new();
   eina_strbuf_append(joinstr,
                      eina_json_string_get(eina_json_pair_value_get(tmp)));

   eina_strbuf_append(joinstr, eina_json_pair_name_get(tmp));
   fail_unless(eina_json_string_set(eina_json_pair_value_get(tmp),
                                    eina_strbuf_string_get(joinstr)));

   eina_strbuf_free(joinstr);

   fail_unless(eina_json_object_insert(obj2, 0, "Num2",
                                       eina_json_number_new(56)) != NULL );
   fail_unless(eina_json_object_insert(obj2, 0, "Num3",
                                       eina_json_number_new(57)) != NULL);
   fail_unless(eina_json_object_insert(obj2, 3, "Str3",
                                       eina_json_string_new("new")) != NULL);

   tmp = eina_json_string_new("fail");
   fail_if(eina_json_object_insert(obj2, 10, "fail", tmp));
   eina_json_value_free(tmp);

   int serial;
   fail_unless(eina_json_object_count_get(obj1) == 7);
   for (serial=0;serial<(int)eina_json_object_count_get(obj1);serial++)
     {
        tmp = eina_json_object_nth_get(obj1, serial);
        fail_unless(eina_json_number_get(eina_json_pair_value_get(tmp)) == serial);
        fail_unless((int)atof(eina_json_pair_name_get(tmp)) == serial);
     }

   serial = 0;
   Eina_Iterator *it = eina_json_object_iterator_new(obj1);
   Eina_Json_Value * data;
   EINA_ITERATOR_FOREACH(it, data)
     {
        fail_unless(eina_json_number_get(eina_json_pair_value_get(data)) == serial);
        fail_unless((int)atof(eina_json_pair_name_get(data)) == serial);
        serial++;
     }
   eina_iterator_free(it);

   Eina_Json_Value* japd;
   japd = eina_json_object_append(jobj, "Object5", eina_json_boolean_new(EINA_TRUE));
   fail_unless(eina_json_type_get(japd) == EINA_JSON_TYPE_PAIR);
   fail_unless(eina_json_boolean_get(eina_json_pair_value_get(japd)));

   char* fstr = eina_json_format_string_get(jobj, EINA_JSON_FORMAT_PACKED);
   fail_if(strcmp(fstr, jstr_object_after));
   free(fstr);

   Eina_Json_Value *treeobj = eina_json_parse(jstr_object_tree);
   fail_if(eina_json_object_value_get(treeobj));
   fail_if(eina_json_object_value_get(treeobj, "Obj"));
   fail_if(eina_json_object_value_get(treeobj, "Obj1", "Obj"));
   tmp = eina_json_object_value_get(treeobj, "Obj1", "Obj1_2");
   fail_unless((tmp != NULL));
   fail_unless((eina_json_number_get(tmp) == 12));
   tmp = eina_json_object_value_get(treeobj, "Obj2" );
   fail_unless((tmp != NULL));
   fail_unless((eina_json_number_get(tmp) == 2));
   tmp = eina_json_null_new();
   eina_json_object_insert(treeobj, 0, "Ent", tmp);
   fail_if(eina_json_object_insert(jobj, 0, "Ent", tmp));
   fail_if(eina_json_object_append(jobj, "Ent", tmp));
   fail_if(eina_json_string_new(NULL));
   eina_json_value_free(treeobj);

   eina_json_value_free(jobj);
   eina_shutdown();
}
END_TEST

START_TEST(eina_json_array_test)
{
   eina_init();

   Eina_Json_Value * jobj = eina_json_parse(jstr_array_before);
   fail_if(!jobj);

   Eina_Json_Value * tmp;

   Eina_Json_Value *arr1 = eina_json_pair_value_get(eina_json_object_nth_get(jobj, 0));
   Eina_Json_Value *arr2 = eina_json_pair_value_get(eina_json_object_nth_get(jobj, 1));
   Eina_Json_Value *arr3 = eina_json_pair_value_get(eina_json_object_nth_get(jobj, 2));
   Eina_Json_Value *arr4 = eina_json_pair_value_get(eina_json_object_nth_get(jobj, 3));

   fail_if(eina_json_array_nth_get(arr4, 0));

   tmp = eina_json_number_new(1);
   fail_if(eina_json_array_insert(arr4, 1, tmp));
   eina_json_value_free(tmp);

   fail_unless(eina_json_array_insert(arr4, 0, eina_json_number_new(1)) != NULL);
   fail_unless(eina_json_array_nth_get(arr4, 0) != NULL);

   tmp = eina_json_number_new(1);
   fail_if(eina_json_array_insert(arr4, 1, tmp));
   eina_json_value_free(tmp);

   fail_if(eina_json_array_nth_get(arr3, 11));
   fail_if(eina_json_array_nth_remove(arr3, 11));
   fail_if(eina_json_array_nth_remove(arr3, 7));
   fail_if(eina_json_array_count_get(arr3)!=6);
   while(eina_json_array_count_get(arr3))
     fail_unless(eina_json_array_nth_remove(arr3, 0));

   fail_if(eina_json_array_nth_remove(arr3, 0));
   fail_if(eina_json_array_nth_get(arr3, 0));

   fail_if(eina_json_type_get(eina_json_array_nth_get(arr2, 5)) != EINA_JSON_TYPE_NULL);
   fail_unless(eina_json_array_nth_remove(arr2, 1));
   fail_if(eina_json_type_get(eina_json_array_nth_get(arr2, 4)) != EINA_JSON_TYPE_NULL);

   tmp = eina_json_array_nth_get(arr2, 1);
   fail_unless(eina_json_boolean_set(tmp, !eina_json_boolean_get(tmp)));

   tmp = eina_json_array_nth_get(arr2, 3);
   fail_unless(eina_json_boolean_set(tmp, !eina_json_boolean_get(tmp)));

   tmp = eina_json_array_nth_get(arr2, 0);
   fail_unless(eina_json_number_set(tmp, eina_json_number_get(tmp)+100));

   tmp = eina_json_array_nth_get(arr2, 2);
   fail_if(strcmp(eina_json_string_get(tmp), "Hello"));
   fail_unless(eina_json_string_set(tmp, "Bye"));

   fail_unless(eina_json_array_insert(arr2, 0, eina_json_number_new(56)) != NULL);
   fail_unless(eina_json_array_insert(arr2, 0, eina_json_number_new(57)) != NULL);
   fail_unless(eina_json_array_insert(arr2, 3, eina_json_string_new("new")) != NULL);

   tmp = eina_json_string_new("fail");
   fail_if(eina_json_array_insert(arr2, 10, tmp));
   eina_json_value_free(tmp);

   int serial;
   fail_unless(eina_json_array_count_get(arr1) == 7);
   for (serial=0;serial<(int)eina_json_array_count_get(arr1);serial++)
     fail_if(eina_json_number_get(eina_json_array_nth_get(arr1, serial)) != serial);

   serial = 0;
   Eina_Iterator *it = eina_json_array_iterator_new(arr1);
   Eina_Json_Value * data;
   EINA_ITERATOR_FOREACH(it, data)
      fail_if(eina_json_number_get(data) != serial++);

   eina_iterator_free(it);
   char* fstr = eina_json_format_string_get(jobj, EINA_JSON_FORMAT_PACKED);
   fail_if(strcmp(fstr, jstr_array_after));
   free(fstr);

   eina_json_value_free(jobj);
   eina_shutdown();
}
END_TEST

void
eina_test_json(TCase *tc)
{
   tcase_add_test(tc, eina_json_parse_test);
   tcase_add_test(tc, eina_json_context_dom_parse_test);
   tcase_add_test(tc, eina_json_context_sax_parse_test);
   tcase_add_test(tc, eina_json_object_test);
   tcase_add_test(tc, eina_json_array_test);
}

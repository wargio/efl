//Compile with:
//gcc -g eina_json_03.c -o eina_json_03 `pkg-config --cflags --libs eina`

// This example demonstrates a simple use of a json context SAX parsing
// with callback function which prints indented json objects

#include "Eina.h"

#define DOERROR(x,args...) { printf(x,## args); return 1; }
#define BUFFSIZE 100

typedef struct
{
   Eina_Strbuf *text;
   unsigned long parent_idx;
} Sax_Parser_Data;

char *json_err_name[]={"No error","Lexical error","Syntax Error","Input Past End"};

char *json_type_string[]={"NULL","NUMBER","STRING","BOOLEAN","PAIR","OBJECT","ARRAY"};

char my_json[] =
"{\n\
\"String\":\"MyJSON\",\n \
\"Array\":[1,2,3,true,5,false,\"\",{},null,[78,\"Hello\"],\"World\"],\n \
\"Object\": { \"Subobj\":{ \"Sub1\":null,\"Sub2\":56} }\n \
}";

void *
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
     }

   saxdata->parent_idx++;
   eina_strbuf_append_printf(saxdata->text,
                             "(%p) PARENT(%p) TYPE:%s",
                             (void*)saxdata->parent_idx,
                             parent,
                             json_type_string[type]);

   if (strval) eina_strbuf_append_printf(saxdata->text, " = \"%s\"", strval);
   eina_strbuf_append_char(saxdata->text, '\n');

   return (void*)saxdata->parent_idx;
}

int
main(int argc,void **argv)
{
   Sax_Parser_Data mysax;

   eina_init();

   mysax.text = eina_strbuf_new();
   mysax.parent_idx = 1;

   // Its a SAX parsing. We initilize it with our our callback function
   // and the data to be used by the callback.
   Eina_Json_Context *ctx = eina_json_context_sax_new(sax_parser_cb, &mysax);

   //Now parse
   eina_json_context_parse(ctx, my_json);

   //Analize and report results
   Eina_Json_Error jsnerr = eina_json_context_error_get(ctx);
   if (jsnerr)
     {
        printf ("Parsing failed\n");
        printf ("Error %d:%d %s\n", eina_json_context_line_get(ctx),
                eina_json_context_column_get(ctx),
                json_err_name[jsnerr]);
     }
   else if (eina_json_context_completed_get(ctx))
     {
        // If parsing was successful - print the text that our callback produced.
        printf ("File was successfully parsed\n\n");
        printf("%s\n", eina_strbuf_string_get(mysax.text));
     }
   else
     printf ("Parsing was not completed\n");

   eina_strbuf_free(mysax.text);
   eina_json_context_free(ctx);

   eina_shutdown();
   return 0;
}

//Compile with:
//gcc -g eina_json_01.c -o eina_json_01 `pkg-config --cflags --libs eina`

// This example demonstrates a simple use of json context parsing of a complete string with example
// of a result's diagnostics and output.

#include "Eina.h"

#define DOERROR(x,args...) { printf(x,## args); return 1; }
#define BUFFSIZE 100

char *json_err_name[] = {"No error","Lexical error","Syntax Error","Input Past End"};
char my_json[] =
"\
{\n\
\"String\":\"MyJSON\",\n \
\"Array\":[1,2,3,true,5,false,\"\",{},null,[78,\"Hello\"],\"World\"],\n \
\"Object\": { \"Subobj\":{ \"Sub1\":null,\"Sub2\":56} }\n \
}\
";

int
main(int argc,void **argv)
{
   eina_init();

   // Its a DOM parsing. When its done without errors, we'll have a json tree to take.
   Eina_Json_Context *ctx = eina_json_context_dom_new();

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
        // If parsing was successful - take the json tree and print it
        printf ("Successfully parsed\n");
        Eina_Json_Value *jsnval = eina_json_context_dom_tree_take(ctx);
        char *json_output = eina_json_format_string_get(jsnval, EINA_JSON_FORMAT_BASIC);
        eina_json_value_free(jsnval);
        printf("%s\n", json_output);
        free(json_output);
     }
   else
     printf ("Parsing was not completed\n");

   eina_json_context_free(ctx);
   eina_shutdown();

   return 0;
}

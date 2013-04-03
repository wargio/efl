//Compile with:
//gcc -g eina_json_02.c -o eina_json_02 `pkg-config --cflags --libs eina`

// This example reads and parses json text gradually from file by BUFFSIZE chunks.
// Usage: eina_json_02 jsonfile

#include "Eina.h"

#define DOERROR(x,args...) { printf(x,## args); return 1; }
#define BUFFSIZE 100

char *json_err_name[]={"No error","Lexical error","Syntax Error","Input Past End"};
char rbuff[BUFFSIZE] = {0};

int
main(int argc, void **argv)
{
   size_t rd;

   eina_init();

   if (argc < 2) DOERROR("Usage: eina_json_02 filename \n");

   FILE* fp = fopen(argv[1], "r");
   if (!fp) DOERROR("Error openning file file %s\n", (char*)argv[1]);

   // Its a DOM parsing. When its done without errors, we'll have a json tree to take.
   Eina_Json_Context *ctx = eina_json_context_dom_new();

   // Read and parse json file by BUFFSIZE chunks
   while (eina_json_context_unfinished_get(ctx))
     {
        rd = fread(rbuff, 1, BUFFSIZE,fp);
        eina_json_context_parse_n(ctx, rbuff, rd);

        //End of file reached
        if (rd != BUFFSIZE) break;
     }

   //Report results
   Eina_Json_Error jsnerr = eina_json_context_error_get(ctx);
   if (jsnerr)
     {
        printf ("Parsing failed\n");
        printf ("Error %d:%d %s\n",eina_json_context_line_get(ctx),
                eina_json_context_column_get(ctx),
                json_err_name[jsnerr]);
     }
   else if (eina_json_context_completed_get(ctx))
     {
        // If parsing was successful - take the json tree and print it
        printf ("File %s was successfully parsed \n\n", (char*)argv[1]);
        Eina_Json_Value *jsnval = eina_json_context_dom_tree_take(ctx);
        char *json_output = eina_json_format_string_get(jsnval, EINA_JSON_FORMAT_BASIC);
        eina_json_value_free(jsnval);
        printf("%s\n", json_output);
        free(json_output);
     }
   else
     printf ("Parsing was not completed - file is incomplete\n");

   eina_json_context_free(ctx);
   fclose(fp);

   eina_shutdown();

   return 0;
}

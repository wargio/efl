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

#ifndef EINA_JSON_H_
#define EINA_JSON_H_

#include <stdio.h>
#include <stdlib.h>

#include "eina_config.h"

#include "eina_types.h"
#include "eina_iterator.h"
#include "eina_inlist.h"
#include "eina_array.h"
#include "eina_strbuf.h"
#include "eina_safety_checks.h"

/**
 * @page eina_json_example_01
 * @include eina_json_01.c
 * @example eina_json_01.c
 */

 /**
 * @page eina_json_example_02
 * @include eina_json_02.c
 * @example eina_json_02.c
 */

 /**
 * @page eina_json_example_03
 * @include eina_json_03.c
 * @example eina_json_03.c
 */

/**
 * @page eina_json_dom_example_page @b Dom @b Parsing
 * @dontinclude eina_json_01.c
 * @n
 * DOM parsing is a method of parsing the whole text document into a convinient
 * data structure. In our case we'll have a @c Eina_Json_Value* pointer to a
 * root json object after we done.
 *
 * So, we define a json string @c my_json
 * @skip my_json
 * @until ;
 *
 * Next we initilize parse context instance @c ctx
 * @skipline Eina_Json_Context
 *
 * and now we can parse @c my_json string
 * @skipline eina_json_context_parse
 *
 * Lets see if error occured
 * @skipline Eina_Json_Error
 *
 * Since no error occurred @ref eina_json_context_error_get() will return
 * EINA_JSON_ERROR_NONE. In our case the parsing was completed successfully so
 * @ref eina_json_context_completed_get() will return true and the next code
 * will be executed
 * @skip eina_json_context_completed_get
 * @until }
 *
 * Since DOM parsing was successful we can now use the function
 * @ref eina_json_context_dom_tree_take in order to get the pointer to a root
 * object. Once we "took" the tree, we explicitly own it and its our
 * responsibility to free it with @ref eina_json_value_free(). We can
 * now traverse, and fully manipulate the tree pointed by @c jsnval with
 * @ref Eina_Json_Tree_Group "Eina_Json_Value API". In this example we
 * just print the tree to screen.
 *
 * And after we done we free the @c ctx instance
 * @skipline eina_json_context_free(ctx);
 *
 * You can see the full source code
 * @ref eina_json_example_01 "here".
 */

/**
 * @page eina_json_sax_example_page @b Sax @b Parsing
 * @dontinclude eina_json_03.c
 * @n
 * Unlike DOM parser, SAX parser doesn't parses the text document into a
 * defined data structure, but instead notifies via callback function about each
 * element and node it encounters. In general,API-wise it works pretty much the
 * same as the @ref eina_json_dom_example_page "DOM" , the main difference that
 * you don't get to @ref eina_json_context_dom_tree_take at the end of successful
 * parsing. What your callback function produces is what you get.
 *
 * First we define a @c struct to use it as data structure for the callback
 * @skip typedef
 * @until Sax_Parser_Data
 *
 * Again, we define a json string @c my_json
 * @skip my_json
 * @until ;
 *
 * Now we define the callback function ( @ref Eina_Json_Parser_Cb )
 * @skip void
 * @until {
 *
 * The callback function takes 4 parameters:
 * @li @c type - the type of json object that was currently parsed
 * @li @c parent - a unique parent's id (usually pointer) representing
 * the parent of this object. If NULL then this is a root object.
 * (Explained further below).
 * @li @c text - a text data of the object. Depends on @c type as follows:
 * @par
 * @c type = @c EINA_JSON_TYPE_STRING @c text holds the string @n
 * @c type = @c EINA_JSON_TYPE_PAIR @c text holds the name of key value @n
 * @c type = @c EINA_JSON_TYPE_NUMBER @c text holds the string of
 * a number (always legal) @n
 * @c type = @c EINA_JSON_TYPE_BOOLEAN first letter of @c text
 * (@c *text) @c 'f' = false, @c 't' = true @n
 *
 * @li @c data - data that is passed to the callback
 *
 * The return value of this callback function is a void* parent id. So as
 * you might have guessed, the @c parent parameter that you receive in the
 * callback is the one that was returned from that callback earlier. If you
 * parsing a json array with 2 numbers @a '[2,3]' it will look like that:
 * You get first call to callback identifying array with parameters
 * @c type = @c EINA_JSON_TYPE_ARRAY and @c parent = Some previous Id... .
 * Your callback allocates the array in memory with address 0x001EDFC,
 * (you save this address your private @c data) and return it. Next
 * call for first element in the array which is number 2,
 * with parameters:  @c type = @c EINA_JSON_TYPE_NUMBER
 * and @c parent = 0x001EDFC (right,from before) @c text = "2". The
 * following call for a second element will be @c type =
  @c EINA_JSON_TYPE_NUMBER and @c parent = 0x001EDFC
 * @c text = "3". @n
 * While its makes most sense that @ parent id will be a (void*) pointer,
 * it doesn't necessary have to be one. As long as you have means to
 * identify parents for their elements and common language between your
 * callback and the parser stack its all good. Another thing about
 * the return value, is that its only meaningful when processing
 * parent types:
 * EINA_JSON_TYPE_PAIR/EINA_JSON_TYPE_ARRAY/EINA_JSON_TYPE_OBJECT.
 * Any other types can return any value (like (void*)1) except NULL.
 * Returning NULL from the callback function will signal the parser
 * that and error had occurred and @c EINA_JSON_ERROR_SYNTAX_TOKEN will
 * be raised and the parser will stop.

 * Now for the rest of our callback function:
 * @until return
 * @until }

 * As you can see, our callback basically creates a string inside
 * @c data->text which represents the elements it parses and the parent
 * id is an increment of data->parent_idx.

 * Now for the main function. First we define a variable to be passed as
 * @c *data to our callback.
 * @until Sax_Parser_Data

 * Then we initialize it.
 * @skip eina
 * @until parent_idx
 *
 * Now create a SAX parse context instance and pass it our callback
 * function and @c sax_parser_cb and reference to @c mysax as data.
 * @skipline Eina_Json_Context
 *
 * Now we parse the the json text:
 * @skipline eina_json_context_parse
 *
 * Upon successful completion we print our string in @c mysax
 * @skip eina_json_context_completed_get
 * @until }
 *
 * Not to forget to free the context
 * @skipline eina_json_context_free
 *
 * You can see the full source code
 * @ref eina_json_example_02 "here".
 */

/**
 * @defgroup Eina_Json_Group Json
 * Json format parser and data structure
 *
 * This module provides a complete set of tools for a Json format.
 * It offers json @ref eina_json_sax_example_page "SAX" and
 * @ref eina_json_dom_example_page "DOM" parser and json tree
 * creation, traversal, manipulation and textual output abilities.
 *
 * @b Parsing @n
 * While still offering a plain run-of-a-mill @ref eina_json_parse "eina_json_parse("
 * @ref eina_json_parse_n "_n )" which takes a @a complete json text,parses it and
 * then returns a json tree on success or NULL on failure,the main feature of this
 * module is the context  parsing function(s) @ref eina_json_context_parse
 * "eina_json_context_parse("  @ref eina_json_context_parse_n "_n )". Context parsing is
 * using a @ref Eina_Json_Context instance to parse a single json root object.
 * The instance is serving as a state keeper of this parsing and this gives us
 * extended abilities to:
 * @li Stream parsing - parsing a single json root object not necessarily in one piece
 * but as a series of sequential parts.
 * @li Configuration for a specific parsing.
 * @li Extended diagnostics and probing for a state of a parsing.
 * @li Easy to add any future API and properties for @ref Eina_Json_Context.
 *
 * A small example. Lets say we have a small json we want to parse:
 * @code
 * {
 *   "Json1":"Hello",
 *   "Json2":"World",
 * }
 * @endcode
 *
 *  We create new @c Eina_Json_Context instance @c called ctx
 * @code
 *    Eina_Json_Context *ctx = eina_json_context_dom_new();
 * @endcode
 * (Note: We choose @ref eina_json_context_dom_new for this example,
 * but we could've initialized @c ctx with
 * @ref eina_json_context_sax_new just as well.)
 *
 * Then we can parse this json as whole:
 * @code
 *    eina_json_context_parse(ctx, "{\"Json1\":\"Hello\",\"Json2\":\"World\"}");
 * @endcode
 *
 * Or as a stream of sequential parts:
 * @code
 *    eina_json_context_parse(ctx, "{\"Json1\":\"H");
 *    eina_json_context_parse(ctx, "ell");
 *    eina_json_context_parse(ctx, "o\",\"Json2\":\"World\"}");
 * @endcode
 *
 * (The examples above parses null-terminated strings.
 * If it isn't your case, use @ref eina_json_context_parse_n
 * with a length of a string)
 *
 * At any time, @c ctx can be probed for its status. Test If parsing was
 * completed successfully, an error occurred or neither (waits further input)
 * and so on with @ref eina_json_context_error_get,
 * @ref eina_json_context_completed_get etc. This @ref eina_json_example_02
 * "example" shows the use of sequential parsing when reading json from file
 * and parsing it in X-sized chunks.
 *
 * Please refer to these examples for detailed explanation:
 * @li @ref eina_json_dom_example_page "DOM parsing"
 * @li @ref eina_json_sax_example_page "SAX parsing"
 */

/**
 * @addtogroup Eina_Tools_Group Tools
 *
 * @{
 */

/**
 * @defgroup Eina_Json_Group Json
 *
 * @{
 */

typedef struct _Eina_Json_Context Eina_Json_Context;
typedef struct _Eina_Json_Value Eina_Json_Value;
typedef struct _Eina_Json_Output_Options Eina_Json_Output_Options;

/**
 * @typedef Eina_Json_Format
 * Determinates how a json tree will be converted to text
 */
typedef enum _Eina_Json_Format
{
   EINA_JSON_FORMAT_PACKED,/**< Prints json in one line no spaces between delimiters */
   EINA_JSON_FORMAT_BASIC/**< Nice, delimitered and indented human readable format */
} Eina_Json_Format;

typedef enum _Eina_Json_Type
{
    EINA_JSON_TYPE_NULL,
    EINA_JSON_TYPE_NUMBER,
    EINA_JSON_TYPE_STRING,
    EINA_JSON_TYPE_BOOLEAN,
    EINA_JSON_TYPE_PAIR,
    EINA_JSON_TYPE_OBJECT,
    EINA_JSON_TYPE_ARRAY
} Eina_Json_Type;

/**
 * @typedef Eina_Json_Error
 * Json parser possible error states
 */
typedef enum _Eina_Json_Error
{
    EINA_JSON_ERROR_NONE,/**< No error */
    EINA_JSON_ERROR_LEX_TOKEN,/**< Error in lexical analysis */
    EINA_JSON_ERROR_SYNTAX_TOKEN,/**< Error in syntax analysis */
    EINA_JSON_ERROR_PAST_END/**< Input was entered to the parser after its already successfully parsed a json root */
} Eina_Json_Error;

typedef void*(*Eina_Json_Parser_Cb)(Eina_Json_Type type, void *parent, const char *text, void*data);

/**
 * @section Eina_Json_Parser_Group "Json Parser API"
 *
 * @{
 */


/**
 * @brief Create new json DOM parser context
 *
 * @return Newly created json parser context on success or @c NULL on failure
 *
 */
EAPI Eina_Json_Context *eina_json_context_dom_new() EINA_WARN_UNUSED_RESULT;


/**
 * @brief @brief Create new json DOM parser context.
 *
 * @param cb The SAX parser @ref eina_json_sax_example_page "callback".
 * Must not be NULL
 * @param data The data to be passed to the callback (@p cb)
 * @return Newly created json parser context on success or @c NULL on failure
 *
 */
EAPI Eina_Json_Context *eina_json_context_sax_new(Eina_Json_Parser_Cb cb, void *data) EINA_WARN_UNUSED_RESULT;


/**
 * @brief Reset the parser context to its initial state.
 *
 * @param ctx The json parser context to be reset
 *
 * Resets the paser context to the state it was when it was just created
 * with @c eina_json_context_dom_new or @c eina_json_context_sax_new and
 * makes it ready to parse a new json text.
 * Use this function when you want to recycle an already existing json
 * context instead of allocating a new one.
 */
EAPI void eina_json_context_reset(Eina_Json_Context *ctx);


/**
 * @brief Free the jason parser context.
 *
 * @param ctx The json parser context to be freed
 *
 * Will free the allocated context with and all of its resourses
 * no matter on what parsing stage it is.
 */
EAPI void eina_json_context_free(Eina_Json_Context *ctx);


/**
 * @brief Returns the current line in a json text document being parsed
 *
 * @param ctx The json parser context with which the text is parsed
 * @return Line number
 *
 * Returns the current location inside a parced text line-wise.
 * As any text document the line count starts from 1.
 */
EAPI unsigned eina_json_context_line_get(Eina_Json_Context *ctx);


/**
 * @brief Returns the current column in a json text document being parsed
 *
 * @param ctx The json parser context with which the text is parsed
 * @return Column number
 *
 * Returns the current location inside a parced text column-wise.
 * As any text document the column count starts from 1.
 */
EAPI unsigned eina_json_context_column_get(Eina_Json_Context *ctx);


/**
 * @brief Returns the error state of parse context
 *
 * @param ctx The json parser context to be probed for error state.
 * @return Error type of @ref Eina_Json_Error.
 *
 * Probe the json parser context for error. If last @c eina_json_context_parse(_n)
 * resulted in an error, it will be returned by this function. If no error occured
 * @c EINA_JSON_ERROR_NONE will be returned.
 *
 */
EAPI Eina_Json_Error eina_json_context_error_get(Eina_Json_Context *ctx);


/**
 * @brief Returns whether a context parsing was succesefully completed
 *
 * @param ctx The json parser context to be probed for complete state.
 * @return @c EINA_TRUE if completed, @c EINA_FALSE is not.
 *
 * Returns true when a single json root was parsed without errors.
 * @note Any input of non blank characters after parsing was completed
 * will result in an @c EINA_ERROR_PAST_END error.
 *
 */
EAPI Eina_Bool eina_json_context_completed_get(Eina_Json_Context *ctx);


/**
 * @brief Returns whether a context parsing awaits further input to complete.
 *
 * @param ctx The json parser context to be probed for unfinished state.
 * @return @c EINA_TRUE if awaits further input or @c EINA_FALSE if not.
 *
 * Basically, this function return true when context parser in neither completed
 * and neither errorneous. Usefull in read loops.
 *
 */
EAPI Eina_Bool eina_json_context_unfinished_get(Eina_Json_Context *ctx);


/**
 * @brief "Takes" parsed json DOM tree from parsing context and returns it.
 *
 * @param ctx The sucessefully parsed DOM parser context.
 * @return Json tree on sucess or @c NULL on error.
 *
 * Once a DOM context parsing was succesefully completed, use this function to
 * obtain its json tree. After the tree is returned and recived into your variable,
 * it will be unreferenced by the json parse context. It will be your responsibility to
 * free it with @ref eina_json_value_free once you "took" it. This function will return
 * @c NULL and will issue an appropriate error message if the @p ctx is not a dom context,
 * parseing is incomplete or errorneous, or if the tree was already "taken".
 *
 */
EAPI Eina_Json_Value *eina_json_context_dom_tree_take(Eina_Json_Context *ctx) EINA_WARN_UNUSED_RESULT;


/**
 * @brief Parse json text into a a json tree.
 *
 * @param text Null terminated string of complete json root object
 * @return A json tree on sucesses, @c NULL otherwise
 *
 * A simple json parsing function. On sucess returns a newly allocated json tree.
 * If parsing fails for whatsoverver reason, @c NULL will be returned.
 * @note Its your resonsibility to free the returned tree with
 * @ref eina_json_value_free
 *
 */
EAPI Eina_Json_Value *eina_json_parse(const char *text);


/**
 * @brief Parse json text of a maximum specified length into a a json tree.
 *
 * @param text String of complete json root object
 * @param text_len The length of @p text
 * @return A json tree on sucesses, @c NULL otherwise
 *
 * This function is same as @ref eina_json_parse. Use it when you have strings
 * of specified length and not necceserelly null terminated. If however a
 * null-terminating character will occur earlier than @p text_len, it will be
 * the end of string.
 *
 * @note Its your resonsibility to free the returned tree with
 * @ref eina_json_value_free
 *
 */
EAPI Eina_Json_Value *eina_json_parse_n(const char *text, unsigned text_len);


/**
 * @brief Parse json text using a parse context
 *
 * @param ctx Json parsing context
 * @param text Null terminated string of complete or sequantial part of json root object
 * @return @c EINA_FALSE if error occured during parsing, otherwise @c EINA_TRUE
 *
 * This function is the core of this module and its usage is documented in the genearal
 * documentation and examples.
 *
 */
EAPI Eina_Bool eina_json_context_parse(Eina_Json_Context *ctx, const char *text);


/**
 * @brief Parse json text of a maximum specified length using a parse context
 *
 * @param ctx Json parsing context
 * @param text String of complete or sequantial part of json root object
 * @return @c EINA_FALSE if error occured during parsing, otherwise @c EINA_TRUE
 *
 * This function is same as @ref eina_json_context_parse .Use it when you have strings
 * of specified length and not necceserelly null terminated. If however a
 * null-terminating character will occur earlier than @p text_len, it will be
 * the end of string.
 *
 */
EAPI Eina_Bool eina_json_context_parse_n(Eina_Json_Context *ctx, const char *text, unsigned text_len);

/**
 * @brief Turn json tree into json text
 *
 * @param jsnval Json tree
 * @param format A text format you wish your json text to be. See @ref Eina_Json_Format
 * @return Null terminated string of json text
 *
 * Takes a json value object and returns a null terminated string reperesenting the text
 * in a format specified by @p format. Its your responsibility to free the string.
 *
 */
EAPI char * eina_json_format_string_get(Eina_Json_Value *jsnval, Eina_Json_Format format) EINA_WARN_UNUSED_RESULT;

/**
 * @}
 */

/**
 * @section Eina_Json_Tree_Group "Json Value API"
 *
 * @{
 *
 */

/**
 * @brief Creates json number value object
 *
 * @param num Number to initilize json value object with.
 * @return Newly allocated json object of type @c EINA_JSON_TYPE_NUMBER
 *
 */
EAPI Eina_Json_Value *eina_json_number_new(double num) EINA_WARN_UNUSED_RESULT;


/**
 * @brief Creates json string value object
 *
 * @param string String to initilize json value object with.
 * @return Newly allocated json object of type @c EINA_JSON_TYPE_STRING
 *
 */
EAPI Eina_Json_Value *eina_json_string_new(const char *string) EINA_WARN_UNUSED_RESULT;


/**
 * @brief Creates json boolean value object
 *
 * @param boolval Boolean value to initilize json value object with.
 * @return Newly allocated json object of type @c EINA_JSON_TYPE_BOOLEAN
 *
 */
EAPI Eina_Json_Value *eina_json_boolean_new(Eina_Bool boolval) EINA_WARN_UNUSED_RESULT;


/**
 * @brief Creates json null value object
 *
 * @return Newly allocated json object of type @c EINA_JSON_TYPE_NULL
 *
 */
EAPI Eina_Json_Value *eina_json_null_new() EINA_WARN_UNUSED_RESULT;


/**
 * @brief Creates json object container value object
 *
 * @return Newly allocated json object of type @c EINA_JSON_TYPE_OBJECT
 *
 */
EAPI Eina_Json_Value *eina_json_object_new() EINA_WARN_UNUSED_RESULT;


/**
 * @brief Creates json array value object
 *
 * @return Newly allocated json object of type @c EINA_JSON_TYPE_ARRAY
 *
 */
EAPI Eina_Json_Value *eina_json_array_new() EINA_WARN_UNUSED_RESULT;


/**
 * @brief Free json value object
 *
 * Frees allocated json value and its childern (if any) recursevly.
 * @warning If a json object is a child of another object, freeing will fail
 * and appropriate message will be logged.
 *
 */
EAPI void eina_json_value_free(Eina_Json_Value *jsnval);


/**
 * @brief Get the type of json value object
 *
 * @param jsnval Json value object
 * @return @p jsnval type
 *
 */
EAPI Eina_Json_Type eina_json_type_get(Eina_Json_Value *jsnval);


/**
 * @brief Get number value of a json number value object
 *
 * @param jsnval Json value object of type @c EINA_JSON_TYPE_NUMBER
 * @return Number value of a @p jsnval
 *
 */
EAPI double eina_json_number_get(Eina_Json_Value *jsnval);


/**
 * @brief Get string value of a json string value object
 *
 * @param jsnval Json value object of type @c EINA_JSON_TYPE_STRING
 * @return String value of a @p jsnval
 *
 */
EAPI const char *eina_json_string_get(Eina_Json_Value *jsnval);


/**
 * @brief Get boolean value of a json boolean value object
 *
 * @param jsnval Json value object of type @c EINA_JSON_TYPE_BOOLEAN
 * @return Boolean value of a @p jsnval
 *
 */
EAPI Eina_Bool eina_json_boolean_get(Eina_Json_Value *jsnval);


/**
 * @brief Get key name part of a key-value pair
 *
 * @param jsnval Json value object of type @c EINA_JSON_TYPE_PAIR
 * @return Key name string
 *
 */
EAPI const char *eina_json_pair_name_get(Eina_Json_Value *jsnval);


/**
 * @brief Get value part of a key-value pair
 *
 * @param jsnval Json value object of type @c EINA_JSON_TYPE_PAIR
 * @return Json value object reperesenting the value part of key-value pair
 *
 */
EAPI Eina_Json_Value *eina_json_pair_value_get(Eina_Json_Value *jsnval);


/**
 * @brief Set number value to a json number value object
 *
 * @param num Json value object of type @c EINA_JSON_TYPE_NUMBER
 * @param num Number to set json value object to.
 * @return @c EINA_TRUE if setting sucesseded, otherwise EINA_FALSE
 *
 */
EAPI Eina_Bool eina_json_number_set(Eina_Json_Value *jsnval, double num);


/**
 * @brief Set string value to a json string value object
 *
 * @param num Json value object of type @c EINA_JSON_TYPE_STRING
 * @param string String to set json value object to.
 * @return @c EINA_TRUE if setting sucesseded, otherwise EINA_FALSE
 *
 */
EAPI Eina_Bool eina_json_string_set(Eina_Json_Value *jsnval, const char* string);


/**
 * @brief Set boolean value to a json string value object
 *
 * @param num Json value object of type @c EINA_JSON_TYPE_BOOLEAN
 * @param boolval Boolean value to set json value object to.
 * @return @c EINA_TRUE if setting sucesseded, otherwise EINA_FALSE
 *
 */
EAPI Eina_Bool eina_json_boolean_set(Eina_Json_Value *jsnval, Eina_Bool boolval);


/**
 * @brief Get number of key-value pairs inside a json object
 *
 * @param obj Json value object of type @c EINA_JSON_TYPE_OBJECT
 * @return Number of elements (key-value pairs) inside @p obj
 *
 */
EAPI unsigned eina_json_object_count_get(Eina_Json_Value *obj);


/**
 * @brief Get the nth element of json object
 *
 * @param obj Json value object of type @c EINA_JSON_TYPE_OBJECT
 * @param idx Index inside the @p obj
 * @return Jason value object of @c EINA_JSON_TYPE_PAIR or @c NULL if @p idx
 * out of bounds or other possible error
 *
 */
EAPI Eina_Json_Value *eina_json_object_nth_get(Eina_Json_Value *obj, unsigned idx);


/**
 * @brief Append new key-value pair to the end of json object
 *
 * @param obj Json value object of type @c EINA_JSON_TYPE_OBJECT
 * @param keyname Name of key in key-value pair
 * @param jsnval Json value object as a value part of key-value pair. Must NOT be a @c EINA_JSON_TYPE_PAIR
 * @return Reference of newly appended key-value pair inside @p obj, or @c NULL on failure
 *
 * @warning Once json value is appended or inserted into another json object or array its
 * fully belongs to it. It can be freed only by that container and cannot be appended or
 * inserted to any other. Doing so will triger a log message and error value will be returned.
 */
EAPI Eina_Json_Value *eina_json_object_append(Eina_Json_Value *obj, const char *keyname, Eina_Json_Value *jsnval);


/**
 * @brief Insert new key-value pair at nth place inside the json object
 *
 * @param obj Json value object of type @c EINA_JSON_TYPE_OBJECT
 * @param idx Index inside the @p obj to insert the element at
 * @param keyname Name of key in key-value pair
 * @param jsnval Json value object as a value part of key-value pair. Must NOT be
 *  a @c EINA_JSON_TYPE_PAIR
 * @return Reference of newly inserted key-value pair inside @p obj, or @c NULL on failure
 *
 * @note Insertion at index 0 on empty object will append the pair. Insertion when index is out of bounds
 * will return NULL.

 * @warning Once json value is appended or inserted into another json object or array its
 * fully belongs to it. It can be freed only by that container and cannot be appended or
 * inserted to any other. Doing so will triger a log message and error value will be returned.
 *
 */
EAPI Eina_Json_Value *eina_json_object_insert(Eina_Json_Value *obj, unsigned idx, const char *keyname, Eina_Json_Value *jsnval);

/**
 * @brief Remove and free a key-value pair at nth place from the json object
 *
 * @param obj Json value object of type @c EINA_JSON_TYPE_OBJECT
 * @param idx Index inside the @p obj to remove the element at.
 * @return @c EINA_TRUE if removal sucedeed, @c EINA_FALSE otherwise
 *
 * The remove element will be freed with @ref eina_json_value_free - thus all of its
 * children will be freed too recursevly.
 *
 */
EAPI Eina_Bool eina_json_object_nth_remove(Eina_Json_Value *obj, unsigned idx);


/**
 * @brief Get @ref Eina_Iterator to iterate over objects key-value elements.
 *
 * @param obj Json value object of type @c EINA_JSON_TYPE_OBJECT
 * @return Json obect's @ref Eina_Iterator
 *
 * This is the fastest way to iterate over the jason object's elements. Use it
 * like any other Eina containers itterator, dont forgewt to free it in the end.
 * All elements of object are of @c EINA_JSON_TYPE_PAIR type
 *
 */
EAPI Eina_Iterator *eina_json_object_iterator_new(Eina_Json_Value *obj) EINA_WARN_UNUSED_RESULT;


/**
 * @brief Get value assosiated with a given key in json object recursively
 *
 * @param obj Json value object of type @c EINA_JSON_TYPE_OBJECT
 * @param ... List of char* key names leading to the desired object. Must end with @c NULL
 * @return Json value object if found, @c NULL otherwise.
 *
 * Finds the corresponding value in a key-value pair inside json object. This a recursive
 * function so you can specify a number of keys and traverse into the children objects as well.
 * If a key occurs more than once in the same object, only its first occurrence will be returned.
 *
 */
EAPI Eina_Json_Value *eina_json_object_value_get_internal(Eina_Json_Value *obj, ...);


/**
 * @def eina_json_object_value_get
 * A convenience wrapper around eina_json_object_value_get_internal()
 * No @c NULL required at the end of paramenters
 * @see eina_json_object_value_get_internal
 */
#define eina_json_object_value_get(obj, args...) eina_json_object_value_get_internal(obj, ## args, NULL)


/**
 * @brief Get number of elements inside a json array
 *
 * @param arr Json value object of type @c EINA_JSON_TYPE_ARRAY
 * @return Number of elements inside @p arr
 *
 */
EAPI unsigned eina_json_array_count_get(Eina_Json_Value *arr);


/**
 * @brief Get the nth element of json array
 *
 * @param arr Json value object of type @c EINA_JSON_TYPE_ARRAY
 * @param idx Index inside the @p arr
 * @return Json value object or @c NULL if @p idx out of bounds or other possible error
 *
 */
EAPI Eina_Json_Value *eina_json_array_nth_get(Eina_Json_Value *arr, unsigned idx);


/**
 * @brief Append new element to the end of json array
 *
 * @param arr Json value object of type @c EINA_JSON_TYPE_ARRAY
 * @param jsnval Json value object to be appended. Must NOT be a @c EINA_JSON_TYPE_PAIR
 * @return Reference of newly appended elemnt inside @p arr or @c NULL on failure
 *
 * @warning Once json value is appended or inserted into another json object or array its
 * fully belongs to it. It can be freed only by that container and cannot be appended or
 * inserted to any other. Doing so will triger a log message and error value will be returned.
 */
EAPI Eina_Json_Value *eina_json_array_append(Eina_Json_Value *arr, Eina_Json_Value *jsnval);


/**
 * @brief Insert new element at nth place inside the json array
 *
 * @param arr Json value object of type @c EINA_JSON_TYPE_ARRAY
 * @param idx Index inside the @p arr to insert the element at
 * @param jsnval Json value object to be inserted. Must NOT be a @c EINA_JSON_TYPE_PAIR
 * @return Reference of newly inserted element @p arr or @c NULL on failure
 *
 * @note Insertion at index 0 on empty array will append the element. Insertion when index is out of bounds
 * will return NULL.

 * @warning Once json value is appended or inserted into another json object or array its
 * fully belongs to it. It can be freed only by that container and cannot be appended or
 * inserted to any other. Doing so will triger a log message and error value will be returned.
 *
 */
EAPI Eina_Json_Value *eina_json_array_insert(Eina_Json_Value *arr, unsigned idx, Eina_Json_Value *jsnval);


/**
 * @brief Remove and free an element at nth place from the json object
 *
 * @param arr Json value object of type @c EINA_JSON_TYPE_ARRAY
 * @param idx Index inside the @p arr to remove the element at.
 * @return @c EINA_TRUE if removal sucedeed, @c EINA_FALSE otherwise
 *
 * The remove element will be freed with @ref eina_json_value_free - thus all of its
 * children will be freed too, recursevly.
 *
 */
EAPI Eina_Bool eina_json_array_nth_remove(Eina_Json_Value *arr, unsigned idx);


/**
 * @brief Get @ref Eina_Iterator to iterate over array elements.
 *
 * @param arr Json value object of type @c EINA_JSON_TYPE_ARRAY
 * @return Json obect's @ref Eina_Iterator
 *
 * This is the fastest way to iterate over the jason array's elements. Use it
 * like any other Eina containers itterator, dont forget to free it in the end.
 *
 */
EAPI Eina_Iterator *eina_json_array_iterator_new(Eina_Json_Value *arr) EINA_WARN_UNUSED_RESULT;


/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif

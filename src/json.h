// SPDX-License-Identifier: MIT

#pragma once

#include "core.h"

#include <stdbool.h>

// A small JSON reader and writer for the files of the library, see pose_clip.c. The reader turns a text into a tape of
// values, one after the other in the order of the text, each container followed by its contents. Numbers are read with
// additions and multiplications only, so a file gives the same bits on every platform, independent of the locale.

typedef enum nbJsonType
{
	nb_jsonNull,
	nb_jsonBool,
	nb_jsonNumber,
	nb_jsonString,
	nb_jsonArray,
	nb_jsonObject,
} nbJsonType;

// A value on the tape. A container counts its direct children and knows the index after its last descendant, so a reader
// skips it in one step. The members of an object are pairs of a key, a string, and a value.
typedef struct nbJsonValue
{
	nbJsonType type;
	int line;
	int column;
	int count;
	int end;
	bool boolean;
	double number;
	int text;
	int length;
} nbJsonValue;

NB_ARRAY_DECLARE( nbJsonValue, nbJsonValueArray );
NB_ARRAY_DECLARE( char, nbCharArray );

typedef struct nbJsonDocument
{
	nbJsonValueArray values;
	nbCharArray strings;
} nbJsonDocument;

// Read a text. False with the line, column and reason of the first error, the document is empty then.
bool nbJson_Parse( nbJsonDocument* document, const char* text, int length, int* line, int* column, char* message,
				   int messageSize );
void nbJson_Free( nbJsonDocument* document );

// The value at an index of the tape, and the index of the value after it with all its contents
const nbJsonValue* nbJson_Get( const nbJsonDocument* document, int index );
int nbJson_Next( const nbJsonDocument* document, int index );

// The value of a member of an object by its key, -1 without
int nbJson_Find( const nbJsonDocument* document, int object, const char* key );

// The text of a string value, terminated, and whether a string value equals a text
const char* nbJson_String( const nbJsonDocument* document, int index );
bool nbJson_Equals( const nbJsonDocument* document, int index, const char* text );

// Writes into a buffer of the caller like snprintf: counts every character, stores those that fit, and terminates
typedef struct nbJsonWriter
{
	char* buffer;
	int capacity;
	int length;
	int depth;
} nbJsonWriter;

void nbJsonWriter_Text( nbJsonWriter* writer, const char* text );
void nbJsonWriter_String( nbJsonWriter* writer, const char* text );
void nbJsonWriter_Int( nbJsonWriter* writer, int value );
void nbJsonWriter_Float( nbJsonWriter* writer, float value );
void nbJsonWriter_Line( nbJsonWriter* writer );
void nbJsonWriter_Finish( nbJsonWriter* writer );

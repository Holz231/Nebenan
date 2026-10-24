// SPDX-License-Identifier: MIT

#include "json.h"

#include <float.h>
#include <stdio.h>
#include <string.h>

// Deeper nesting than this is no file of the library, and the reader recurses
#define NB_JSON_MAX_DEPTH 64

typedef struct nbJsonParser
{
	nbJsonDocument* document;
	const char* text;
	int length;
	int position;
	int line;
	int lineStart;
	int depth;
	bool failed;
	int errorLine;
	int errorColumn;
	char* message;
	int messageSize;
} nbJsonParser;

static void nbJsonFail( nbJsonParser* p, const char* message )
{
	if ( p->failed )
	{
		return;
	}

	p->failed = true;
	p->errorLine = p->line;
	p->errorColumn = p->position - p->lineStart + 1;
	if ( p->message != NULL && p->messageSize > 0 )
	{
		snprintf( p->message, (size_t)p->messageSize, "%s", message );
	}
}

static int nbJsonPeek( const nbJsonParser* p )
{
	return p->position < p->length ? (unsigned char)p->text[p->position] : -1;
}

static void nbJsonSkipSpace( nbJsonParser* p )
{
	while ( p->position < p->length )
	{
		char c = p->text[p->position];
		if ( c == '\n' )
		{
			p->position += 1;
			p->line += 1;
			p->lineStart = p->position;
		}
		else if ( c == ' ' || c == '\t' || c == '\r' )
		{
			p->position += 1;
		}
		else
		{
			break;
		}
	}
}

static int nbJsonPush( nbJsonParser* p, nbJsonType type )
{
	nbJsonValue value = { 0 };
	value.type = type;
	value.line = p->line;
	value.column = p->position - p->lineStart + 1;
	value.text = -1;
	nbArray_Push( p->document->values, value );
	return p->document->values.count - 1;
}

static void nbJsonPushChar( nbJsonParser* p, char c )
{
	nbArray_Push( p->document->strings, c );
}

// A code point as UTF-8
static void nbJsonPushCodePoint( nbJsonParser* p, uint32_t code )
{
	if ( code < 0x80 )
	{
		nbJsonPushChar( p, (char)code );
	}
	else if ( code < 0x800 )
	{
		nbJsonPushChar( p, (char)( 0xC0 | ( code >> 6 ) ) );
		nbJsonPushChar( p, (char)( 0x80 | ( code & 0x3F ) ) );
	}
	else if ( code < 0x10000 )
	{
		nbJsonPushChar( p, (char)( 0xE0 | ( code >> 12 ) ) );
		nbJsonPushChar( p, (char)( 0x80 | ( ( code >> 6 ) & 0x3F ) ) );
		nbJsonPushChar( p, (char)( 0x80 | ( code & 0x3F ) ) );
	}
	else
	{
		nbJsonPushChar( p, (char)( 0xF0 | ( code >> 18 ) ) );
		nbJsonPushChar( p, (char)( 0x80 | ( ( code >> 12 ) & 0x3F ) ) );
		nbJsonPushChar( p, (char)( 0x80 | ( ( code >> 6 ) & 0x3F ) ) );
		nbJsonPushChar( p, (char)( 0x80 | ( code & 0x3F ) ) );
	}
}

static bool nbJsonHex4( nbJsonParser* p, uint32_t* code )
{
	uint32_t value = 0;
	for ( int i = 0; i < 4; ++i )
	{
		int c = nbJsonPeek( p );
		uint32_t digit;
		if ( '0' <= c && c <= '9' )
		{
			digit = (uint32_t)( c - '0' );
		}
		else if ( 'a' <= c && c <= 'f' )
		{
			digit = (uint32_t)( c - 'a' + 10 );
		}
		else if ( 'A' <= c && c <= 'F' )
		{
			digit = (uint32_t)( c - 'A' + 10 );
		}
		else
		{
			nbJsonFail( p, "expected four hex digits after \\u" );
			return false;
		}
		value = 16 * value + digit;
		p->position += 1;
	}
	*code = value;
	return true;
}

// A string, unescaped into the strings of the document and terminated
static void nbJsonString( nbJsonParser* p, int index )
{
	p->position += 1;
	int start = p->document->strings.count;
	while ( true )
	{
		int c = nbJsonPeek( p );
		if ( c < 0 || c == '\n' )
		{
			nbJsonFail( p, "unterminated string" );
			return;
		}
		if ( c < 0x20 )
		{
			nbJsonFail( p, "control character in a string" );
			return;
		}
		p->position += 1;
		if ( c == '"' )
		{
			break;
		}
		if ( c != '\\' )
		{
			nbJsonPushChar( p, (char)c );
			continue;
		}

		int escape = nbJsonPeek( p );
		p->position += 1;
		switch ( escape )
		{
			case '"':
			case '\\':
			case '/':
				nbJsonPushChar( p, (char)escape );
				break;
			case 'b':
				nbJsonPushChar( p, '\b' );
				break;
			case 'f':
				nbJsonPushChar( p, '\f' );
				break;
			case 'n':
				nbJsonPushChar( p, '\n' );
				break;
			case 'r':
				nbJsonPushChar( p, '\r' );
				break;
			case 't':
				nbJsonPushChar( p, '\t' );
				break;
			case 'u':
			{
				uint32_t code;
				if ( nbJsonHex4( p, &code ) == false )
				{
					return;
				}

				// A surrogate pair makes one code point
				if ( 0xD800 <= code && code < 0xDC00 && nbJsonPeek( p ) == '\\' && p->position + 1 < p->length &&
					 p->text[p->position + 1] == 'u' )
				{
					p->position += 2;
					uint32_t low;
					if ( nbJsonHex4( p, &low ) == false )
					{
						return;
					}
					if ( 0xDC00 <= low && low < 0xE000 )
					{
						code = 0x10000 + ( ( code - 0xD800 ) << 10 ) + ( low - 0xDC00 );
					}
				}
				nbJsonPushCodePoint( p, code );
				break;
			}
			default:
				p->position -= 1;
				nbJsonFail( p, "unknown escape in a string" );
				return;
		}
	}

	nbJsonValue* value = p->document->values.data + index;
	value->text = start;
	value->length = p->document->strings.count - start;
	nbJsonPushChar( p, '\0' );
}

static bool nbJsonIsDigit( int c )
{
	return '0' <= c && c <= '9';
}

// A number, read with additions and multiplications only, so the same text gives the same bits on every platform and
// in every locale. Up to 19 significant digits and powers of ten up to 22 are exact, so a float written with 9
// significant digits comes back to the same bits.
static void nbJsonNumber( nbJsonParser* p, int index )
{
	static const double powers[23] = { 1e0,	 1e1,  1e2,	 1e3,  1e4,	 1e5,  1e6,	 1e7,  1e8,	 1e9,  1e10, 1e11,
									   1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22 };

	bool negative = false;
	if ( nbJsonPeek( p ) == '-' )
	{
		negative = true;
		p->position += 1;
	}

	if ( nbJsonIsDigit( nbJsonPeek( p ) ) == false )
	{
		nbJsonFail( p, "expected a digit" );
		return;
	}

	uint64_t mantissa = 0;
	int significant = 0;
	int exponent = 0;

	// The integer part, without leading zeros beyond the first
	if ( nbJsonPeek( p ) == '0' )
	{
		p->position += 1;
	}
	else
	{
		while ( nbJsonIsDigit( nbJsonPeek( p ) ) )
		{
			int digit = nbJsonPeek( p ) - '0';
			if ( significant < 19 )
			{
				mantissa = 10 * mantissa + (uint64_t)digit;
				significant += mantissa != 0 ? 1 : 0;
			}
			else
			{
				exponent += 1;
			}
			p->position += 1;
		}
	}

	if ( nbJsonPeek( p ) == '.' )
	{
		p->position += 1;
		if ( nbJsonIsDigit( nbJsonPeek( p ) ) == false )
		{
			nbJsonFail( p, "expected a digit after the decimal point" );
			return;
		}
		while ( nbJsonIsDigit( nbJsonPeek( p ) ) )
		{
			int digit = nbJsonPeek( p ) - '0';
			if ( significant < 19 )
			{
				mantissa = 10 * mantissa + (uint64_t)digit;
				significant += mantissa != 0 ? 1 : 0;
				exponent -= 1;
			}
			p->position += 1;
		}
	}

	if ( nbJsonPeek( p ) == 'e' || nbJsonPeek( p ) == 'E' )
	{
		p->position += 1;
		int sign = 1;
		if ( nbJsonPeek( p ) == '+' || nbJsonPeek( p ) == '-' )
		{
			sign = nbJsonPeek( p ) == '-' ? -1 : 1;
			p->position += 1;
		}
		if ( nbJsonIsDigit( nbJsonPeek( p ) ) == false )
		{
			nbJsonFail( p, "expected a digit in the exponent" );
			return;
		}
		int value = 0;
		while ( nbJsonIsDigit( nbJsonPeek( p ) ) )
		{
			value = value < 10000 ? 10 * value + ( nbJsonPeek( p ) - '0' ) : value;
			p->position += 1;
		}
		exponent += sign * value;
	}

	double number = (double)mantissa;
	if ( mantissa != 0 && exponent > 308 )
	{
		nbJsonFail( p, "number out of range" );
		return;
	}
	if ( mantissa == 0 || exponent < -360 )
	{
		number = 0.0;
	}
	else if ( exponent > 0 )
	{
		if ( exponent <= 22 && mantissa < ( (uint64_t)1 << 53 ) )
		{
			number *= powers[exponent];
		}
		else
		{
			for ( int i = 0; i < exponent; ++i )
			{
				number *= 10.0;
			}
		}
	}
	else if ( exponent < 0 )
	{
		if ( -exponent <= 22 && mantissa < ( (uint64_t)1 << 53 ) )
		{
			number /= powers[-exponent];
		}
		else
		{
			for ( int i = 0; i < -exponent; ++i )
			{
				number /= 10.0;
			}
		}
	}

	if ( number > DBL_MAX )
	{
		nbJsonFail( p, "number out of range" );
		return;
	}
	p->document->values.data[index].number = negative ? -number : number;
}

static bool nbJsonLiteral( nbJsonParser* p, const char* word )
{
	int length = (int)strlen( word );
	if ( p->position + length > p->length || memcmp( p->text + p->position, word, (size_t)length ) != 0 )
	{
		nbJsonFail( p, "unknown word" );
		return false;
	}
	p->position += length;
	return true;
}

static void nbJsonValueAt( nbJsonParser* p );

static void nbJsonContainer( nbJsonParser* p, int index, bool object )
{
	char close = object ? '}' : ']';
	p->position += 1;
	p->depth += 1;
	if ( p->depth > NB_JSON_MAX_DEPTH )
	{
		nbJsonFail( p, "nested too deep" );
		return;
	}

	int count = 0;
	nbJsonSkipSpace( p );
	if ( nbJsonPeek( p ) == close )
	{
		p->position += 1;
	}
	else
	{
		while ( p->failed == false )
		{
			nbJsonSkipSpace( p );
			if ( object )
			{
				if ( nbJsonPeek( p ) != '"' )
				{
					nbJsonFail( p, "expected a key in quotes" );
					return;
				}
				int key = nbJsonPush( p, nb_jsonString );
				nbJsonString( p, key );
				p->document->values.data[key].end = p->document->values.count;
				nbJsonSkipSpace( p );
				if ( nbJsonPeek( p ) != ':' )
				{
					nbJsonFail( p, "expected a colon after the key" );
					return;
				}
				p->position += 1;
			}

			nbJsonSkipSpace( p );
			nbJsonValueAt( p );
			count += 1;
			nbJsonSkipSpace( p );

			int c = nbJsonPeek( p );
			if ( c == ',' )
			{
				p->position += 1;
			}
			else if ( c == close )
			{
				p->position += 1;
				break;
			}
			else
			{
				nbJsonFail( p, object ? "expected a comma or a closing brace" : "expected a comma or a closing bracket" );
				return;
			}
		}
	}

	p->depth -= 1;
	nbJsonValue* value = p->document->values.data + index;
	value->count = count;
	value->end = p->document->values.count;
}

static void nbJsonValueAt( nbJsonParser* p )
{
	if ( p->failed )
	{
		return;
	}

	int c = nbJsonPeek( p );
	int index;
	switch ( c )
	{
		case '{':
			index = nbJsonPush( p, nb_jsonObject );
			nbJsonContainer( p, index, true );
			return;
		case '[':
			index = nbJsonPush( p, nb_jsonArray );
			nbJsonContainer( p, index, false );
			return;
		case '"':
			index = nbJsonPush( p, nb_jsonString );
			nbJsonString( p, index );
			break;
		case 't':
		case 'f':
			index = nbJsonPush( p, nb_jsonBool );
			if ( nbJsonLiteral( p, c == 't' ? "true" : "false" ) )
			{
				p->document->values.data[index].boolean = c == 't';
			}
			break;
		case 'n':
			index = nbJsonPush( p, nb_jsonNull );
			nbJsonLiteral( p, "null" );
			break;
		default:
			if ( c == '-' || nbJsonIsDigit( c ) )
			{
				index = nbJsonPush( p, nb_jsonNumber );
				nbJsonNumber( p, index );
				break;
			}
			nbJsonFail( p, c < 0 ? "unexpected end of the text" : "unexpected character" );
			return;
	}
	p->document->values.data[index].end = p->document->values.count;
}

bool nbJson_Parse( nbJsonDocument* document, const char* text, int length, int* line, int* column, char* message,
				   int messageSize )
{
	memset( document, 0, sizeof( nbJsonDocument ) );
	nbJsonParser p = { 0 };
	p.document = document;
	p.text = text;
	p.length = text != NULL ? length : 0;
	p.line = 1;
	p.message = message;
	p.messageSize = messageSize;

	// A byte order mark is no text
	if ( p.length >= 3 && memcmp( text, "\xEF\xBB\xBF", 3 ) == 0 )
	{
		p.position = 3;
		p.lineStart = 3;
	}

	nbJsonSkipSpace( &p );
	nbJsonValueAt( &p );
	nbJsonSkipSpace( &p );
	if ( p.failed == false && p.position < p.length )
	{
		nbJsonFail( &p, "text after the end of the value" );
	}

	if ( p.failed )
	{
		if ( line != NULL )
		{
			*line = p.errorLine;
		}
		if ( column != NULL )
		{
			*column = p.errorColumn;
		}
		nbJson_Free( document );
		return false;
	}
	return true;
}

void nbJson_Free( nbJsonDocument* document )
{
	nbArray_Free( document->values );
	nbArray_Free( document->strings );
}

const nbJsonValue* nbJson_Get( const nbJsonDocument* document, int index )
{
	NB_ASSERT( 0 <= index && index < document->values.count );
	return document->values.data + index;
}

int nbJson_Next( const nbJsonDocument* document, int index )
{
	return nbJson_Get( document, index )->end;
}

int nbJson_Find( const nbJsonDocument* document, int object, const char* key )
{
	if ( object < 0 || nbJson_Get( document, object )->type != nb_jsonObject )
	{
		return -1;
	}

	int member = object + 1;
	int count = nbJson_Get( document, object )->count;
	for ( int i = 0; i < count; ++i )
	{
		int value = member + 1;
		if ( nbJson_Equals( document, member, key ) )
		{
			return value;
		}
		member = nbJson_Next( document, value );
	}
	return -1;
}

const char* nbJson_String( const nbJsonDocument* document, int index )
{
	const nbJsonValue* value = nbJson_Get( document, index );
	return value->type == nb_jsonString ? document->strings.data + value->text : "";
}

bool nbJson_Equals( const nbJsonDocument* document, int index, const char* text )
{
	const nbJsonValue* value = nbJson_Get( document, index );
	int length = (int)strlen( text );
	return value->type == nb_jsonString && value->length == length &&
		   memcmp( document->strings.data + value->text, text, (size_t)length ) == 0;
}

//----------------------------------------------------------------------------------------------------------------------
// Writer

static void nbJsonWriter_Append( nbJsonWriter* writer, const char* text, int length )
{
	for ( int i = 0; i < length; ++i )
	{
		if ( writer->length + 1 < writer->capacity )
		{
			writer->buffer[writer->length] = text[i];
		}
		writer->length += 1;
	}
}

void nbJsonWriter_Text( nbJsonWriter* writer, const char* text )
{
	nbJsonWriter_Append( writer, text, (int)strlen( text ) );
}

void nbJsonWriter_String( nbJsonWriter* writer, const char* text )
{
	nbJsonWriter_Append( writer, "\"", 1 );
	for ( const char* c = text; *c != '\0'; ++c )
	{
		unsigned char u = (unsigned char)*c;
		if ( u == '"' || u == '\\' )
		{
			char escaped[2] = { '\\', (char)u };
			nbJsonWriter_Append( writer, escaped, 2 );
		}
		else if ( u < 0x20 )
		{
			char escaped[8];
			snprintf( escaped, sizeof( escaped ), "\\u%04x", u );
			nbJsonWriter_Append( writer, escaped, 6 );
		}
		else
		{
			nbJsonWriter_Append( writer, c, 1 );
		}
	}
	nbJsonWriter_Append( writer, "\"", 1 );
}

void nbJsonWriter_Int( nbJsonWriter* writer, int value )
{
	char text[16];
	int length = snprintf( text, sizeof( text ), "%d", value );
	nbJsonWriter_Append( writer, text, length );
}

// Nine significant digits bring every float back to its bits. A locale with a decimal comma does not reach the file.
void nbJsonWriter_Float( nbJsonWriter* writer, float value )
{
	char text[32];
	int length = 1;
	text[0] = '0';
	if ( value == value && value <= FLT_MAX && value >= -FLT_MAX )
	{
		length = snprintf( text, sizeof( text ), "%.9g", (double)value );
		for ( int i = 0; i < length; ++i )
		{
			text[i] = text[i] == ',' ? '.' : text[i];
		}
	}
	nbJsonWriter_Append( writer, text, length );
}

// A new line, indented by the depth
void nbJsonWriter_Line( nbJsonWriter* writer )
{
	nbJsonWriter_Append( writer, "\n", 1 );
	for ( int i = 0; i < writer->depth; ++i )
	{
		nbJsonWriter_Append( writer, "  ", 2 );
	}
}

void nbJsonWriter_Finish( nbJsonWriter* writer )
{
	if ( writer->capacity > 0 )
	{
		writer->buffer[writer->length < writer->capacity ? writer->length : writer->capacity - 1] = '\0';
	}
}

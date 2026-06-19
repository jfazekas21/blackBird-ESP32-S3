// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
//#include "rtl.h"
#include <stddef.h>
#include <stdint.h>
#include "string.h"
#include "base64.h"
#define splitInt(intVal, bytePos)   (char)((intVal >> (bytePos << 3)) & 0xFF)
#define joinChars(a, b, c, d) (uint32_t)((uint32_t)a + ((uint32_t)b << 8) + ((uint32_t)c << 16) + ((uint32_t)d << 24))
#define U8	unsigned char
// typedef struct BUFFER_TAG
// {
//     unsigned char* buffer;
//     size_t size;
// }BUFFER;
typedef struct BUFFER_TAG* BUFFER_HANDLE;
int encoded_string_size,decoded_buf_size;
B64STRING* pt_String1;
BUFFER * pt_buffer1;
//char *decoded_buf __attribute__ ((section ("STRUCTURES_STACK")));
char decoded_buf[800];
// typedef struct STRING_TAG
// {
//     char* s;
// }STRING;

// typedef struct BUFFER_TAG
// {
//     unsigned char* buffer;
//     size_t size;
// }BUFFER;
/* Codes_SRS_BUFFER_07_021: [B64BUFFER_size shall place the size of the associated buffer in the size variable and return zero on success.] */
int B64BUFFER_size(BUFFER_HANDLE handle, size_t* size)
{
    int result;
    if ((handle == NULL) || (size == NULL))
    {
        /* Codes_SRS_BUFFER_07_022: [B64BUFFER_size shall return a nonzero value for any error that is encountered.] */
        result = __LINE__;
    }
    else
    {
        BUFFER* b = (BUFFER*)handle;
        *size = b->size;
        result = 0;
    }
    return result;
}


/* Codes_SRS_BUFFER_07_019: [B64BUFFER_content shall return the data contained within the BUFFER_HANDLE.] */
int B64BUFFER_content(BUFFER_HANDLE handle, const unsigned char** content)
{
    int result;
    if ((handle == NULL) || (content == NULL))
    {
        /* Codes_SRS_BUFFER_07_020: [If the handle and/or content*is NULL B64BUFFER_content shall return nonzero.] */
        result = __LINE__;
    }
    else
    {
        BUFFER* b = (BUFFER*)handle;
        *content = b->buffer;
        result = 0;
    }
    return result;
}

/* Codes_SRS_BUFFER_07_003: [B64BUFFER_delete shall delete the data associated with the BUFFER_HANDLE along with the Buffer.] */
void B64BUFFER_delete(BUFFER_HANDLE handle)
{
    /* Codes_SRS_BUFFER_07_004: [B64BUFFER_delete shall not delete any BUFFER_HANDLE that is NULL.] */
    if (handle != NULL)
    {
        BUFFER* b = (BUFFER*)handle;
        if (b->buffer != NULL)
        {
            /* Codes_SRS_BUFFER_07_003: [B64BUFFER_delete shall delete the data associated with the BUFFER_HANDLE along with the Buffer.] */
            free(b->buffer);
        }
        free(b);
    }
}
/* Codes_SRS_BUFFER_07_001: [B64BUFFER_new shall allocate a BUFFER_HANDLE that will contain a NULL unsigned char*.] */
BUFFER_HANDLE B64BUFFER_new(void)
{
    BUFFER* temp = (BUFFER*)malloc(sizeof(BUFFER));
    /* Codes_SRS_BUFFER_07_002: [B64BUFFER_new shall return NULL on any error that occurs.] */
    if (temp != NULL)
    {
        temp->buffer = NULL;
        temp->size = 0;
    }
    return (BUFFER_HANDLE)temp;
}
/*return 0 if the buffer was pre-build(that is, had its space allocated)*/
/*else return different than zero*/
/* Codes_SRS_BUFFER_07_005: [B64BUFFER_pre_build allocates size_t bytes of BUFFER_HANDLE and returns zero on success.] */
int B64BUFFER_pre_build(BUFFER_HANDLE handle, size_t size)
{
    int result;
    if (handle == NULL)
    {
        /* Codes_SRS_BUFFER_07_006: [If handle is NULL or size is 0 then B64BUFFER_pre_build shall return a nonzero value.] */
        result = __LINE__;
    }
    else if (size == 0)
    {
        /* Codes_SRS_BUFFER_07_006: [If handle is NULL or size is 0 then B64BUFFER_pre_build shall return a nonzero value.] */
        result = __LINE__;
    }
    else
    {
        BUFFER* b = (BUFFER*)handle;
        if (b->buffer != NULL)
        {
            /* Codes_SRS_BUFFER_07_007: [B64BUFFER_pre_build shall return nonzero if the buffer has been previously allocated and is not NULL.] */
            result = __LINE__;
        }
        else
        {
            if ((b->buffer = (unsigned char*)malloc(size)) == NULL)
            {
                /* Codes_SRS_BUFFER_07_013: [B64BUFFER_pre_build shall return nonzero if any error is encountered.] */
                result = __LINE__;
            }
            else
            {
								free(b->buffer);
                b->size = size;
                result = 0;
            }
        }
    }
    return result;
}




/* Codes_SRS_BUFFER_07_025: [B64BUFFER_u_char shall return a pointer to the underlying unsigned char*.] */
unsigned char* B64BUFFER_u_char(BUFFER_HANDLE handle)
{
    BUFFER* handleData = (BUFFER*)handle;
    unsigned char* result;
    if (handle == NULL || handleData->size == 0)
    {
        /* Codes_SRS_BUFFER_07_026: [B64BUFFER_u_char shall return NULL for any error that is encountered.] */
        /* Codes_SRS_BUFFER_07_029: [B64BUFFER_u_char shall return NULL if underlying buffer size is zero.] */
        result = NULL;
    }
    else
    {
        result = handleData->buffer;
    }
    return result;
}

//=====================================STRINGS.C
//typedef struct STRING_TAG* STRING_HANDLE;
/*this function will allocate a new string with just '\0' in it*/
/*return NULL if it fails*/
/* Codes_SRS_STRING_07_001: [B64STRING_new shall allocate a new STRING_HANDLE pointing to an empty string.] */
STRING_HANDLE B64STRING_new(void)
{
	B64STRING* result;
    if ((result = (B64STRING*)malloc(sizeof(B64STRING))) != NULL)
    {
        if ((result->s = (char*)malloc(1)) != NULL)
        {
            result->s[0] = '\0';
        }
        else
        {
            /* Codes_SRS_STRING_07_002: [B64STRING_new shall return an NULL STRING_HANDLE on any error that is encountered.] */
            free(result);
            result = NULL;
        }
    }
    return (STRING_HANDLE)result;
}
/*this function will return a new STRING with the memory for the actual string passed in as a parameter.*/
/*return NULL if it fails.*/
/* The supplied memory must have been allocated with malloc! */
/* Codes_SRS_STRING_07_006: [B64STRING_new_with_memory shall return a STRING_HANDLE by using the supplied char* memory.] */
STRING_HANDLE B64STRING_new_with_memory(const char* memory)
{
	B64STRING* result;
    if (memory == NULL)
    {
        /* Codes_SRS_STRING_07_007: [B64STRING_new_with_memory shall return a NULL STRING_HANDLE if the supplied char* is NULL.] */
        result = NULL;
    }
    else
    {
        if ((result = (B64STRING*)malloc(sizeof(B64STRING))) != NULL)
        {
            result->s = (char*)memory;
        }
    }
    return (STRING_HANDLE)result;
}
//=====================================BASE_64

typedef struct STRING_TAG* STRING_HANDLE;
static char base64char(unsigned char val)
{
    char result;

    if (val < 26)
    {
        result = 'A' + (char)val;
    }
    else if (val < 52)
    {
        result = 'a' + ((char)val - 26);
    }
    else if (val < 62)
    {
        result = '0' + ((char)val - 52);
    }
    else if (val == 62)
    {
        result = '+';
    }
    else
    {
        result = '/';
    }

    return result;
}

static char base64b16(unsigned char val)
{
    const uint32_t base64b16values[4] = {
        joinChars('A', 'E', 'I', 'M'),
        joinChars('Q', 'U', 'Y', 'c'),
        joinChars('g', 'k', 'o', 's'),
        joinChars('w', '0', '4', '8')
    };
    return splitInt(base64b16values[val >> 2], (val & 0x03));
}

static char base64b8(unsigned char val)
{
    const uint32_t base64b8values = joinChars('A', 'Q', 'g', 'w');
    return splitInt(base64b8values, val);
}

static int base64toValue(char base64character, unsigned char* value)
{
    int result = 0;
    if (('A' <= base64character) && (base64character <= 'Z'))
    {
        *value = base64character - 'A';
    }
    else if (('a' <= base64character) && (base64character <= 'z'))
    {
        *value = ('Z' - 'A') + 1 + (base64character - 'a');
    }
    else if (('0' <= base64character) && (base64character <= '9'))
    {
        *value = ('Z' - 'A') + 1 + ('z' - 'a') + 1 + (base64character - '0');
    }
    else if ('+' == base64character)
    {
        *value = 62;
    }
    else if ('/' == base64character)
    {
        *value = 63;
    }
    else
    {
        *value = 0;
        result = -1;
    }
    return result;
}

static size_t numberOfBase64Characters(const char* encodedString)
{
    size_t length = 0;
    unsigned char junkChar;
    while (base64toValue(encodedString[length],&junkChar) != -1)
    {
        length++;
    }
    return length;
}

/*returns the count of original bytes before being base64 encoded*/
/*notice NO validation of the content of encodedString. Its length is validated to be a multiple of 4.*/
static size_t Base64decode_len(const char *encodedString)
{
    size_t result;
    size_t sourceLength = strlen(encodedString);
    
    if (sourceLength == 0)
    {
        result = 0;
    }
    else
    {
        result = sourceLength / 4 * 3;
        if (encodedString[sourceLength - 1] == '=')
        {
            if (encodedString[sourceLength - 2] == '=')
            {
                result --;
            }
            result--;
        }
    }
    return result;
}

void Base64decode(unsigned char *decodedString, const char *base64String)
{

    size_t numberOfEncodedChars;
    size_t indexOfFirstEncodedChar;
    size_t decodedIndex;

    //
    // We can only operate on individual bytes.  If we attempt to work
    // on anything larger we could get an alignment fault on some
    // architectures
    //

    numberOfEncodedChars = numberOfBase64Characters(base64String);
    indexOfFirstEncodedChar = 0;
    decodedIndex = 0;
    while (numberOfEncodedChars >= 4)
    {
        unsigned char c1;
        unsigned char c2;
        unsigned char c3;
        unsigned char c4;
        (void)base64toValue(base64String[indexOfFirstEncodedChar], &c1);
        (void)base64toValue(base64String[indexOfFirstEncodedChar + 1], &c2);
        (void)base64toValue(base64String[indexOfFirstEncodedChar + 2], &c3);
        (void)base64toValue(base64String[indexOfFirstEncodedChar + 3], &c4);
        decodedString[decodedIndex] = (c1 << 2) | (c2 >> 4);
        decodedIndex++;
        decodedString[decodedIndex] = ((c2 & 0x0f) << 4) | (c3 >> 2);
        decodedIndex++;
        decodedString[decodedIndex] = ((c3 & 0x03) << 6) | c4;
        decodedIndex++;
        numberOfEncodedChars -= 4;
        indexOfFirstEncodedChar += 4;

    }

    if (numberOfEncodedChars == 2)
    {
        unsigned char c1;
        unsigned char c2;
        (void)base64toValue(base64String[indexOfFirstEncodedChar], &c1);
        (void)base64toValue(base64String[indexOfFirstEncodedChar + 1], &c2);
        decodedString[decodedIndex] = (c1 << 2) | (c2 >> 4);
    }
    else if (numberOfEncodedChars == 3)
    {
        unsigned char c1;
        unsigned char c2;
        unsigned char c3;
        (void)base64toValue(base64String[indexOfFirstEncodedChar], &c1);
        (void)base64toValue(base64String[indexOfFirstEncodedChar + 1], &c2);
        (void)base64toValue(base64String[indexOfFirstEncodedChar + 2], &c3);
        decodedString[decodedIndex] = (c1 << 2) | (c2 >> 4);
        decodedIndex++;
        decodedString[decodedIndex] = ((c2 & 0x0f) << 4) | (c3 >> 2);
    }
			decoded_buf_size = decodedIndex+1;
//		pt_buffer1->size=	decodedIndex+1;
}

BUFFER_HANDLE Base64_Decoder(const char* source)
{
    BUFFER_HANDLE result;
    /*Codes_SRS_BASE64_06_008: [If source is NULL then Base64_Decode shall return NULL.]*/
    if (source == NULL)
    {
       //// LogError("invalid parameter const char* source=%p", source);
        result = NULL;
    }
    else
    {
        if ((strlen(source) % 4) != 0)
        {
            /*Codes_SRS_BASE64_06_011: [If the source string has an invalid length for a base 64 encoded string then Base64_Decode shall return NULL.]*/
           // LogError("Invalid length Base64 string!");
            result = NULL;
        }
        else
        {
            if ((result = B64BUFFER_new()) == NULL)
            {
                /*Codes_SRS_BASE64_06_010: [If there is any memory allocation failure during the decode then Base64_Decode shall return NULL.]*/
               // LogError("Could not create a buffer to decoding.");
            }
            else
            {
                size_t sizeOfOutputBuffer = Base64decode_len(source);
                /*Codes_SRS_BASE64_06_009: [If the string pointed to by source is zero length then the handle returned shall refer to a zero length buffer.]*/
                if (sizeOfOutputBuffer > 0)
                {
                    if (B64BUFFER_pre_build(result, sizeOfOutputBuffer) != 0)
                    {
                        /*Codes_SRS_BASE64_06_010: [If there is any memory allocation failure during the decode then Base64_Decode shall return NULL.]*/
                       // LogError("Could not prebuild a buffer for base 64 decoding.");
                        B64BUFFER_delete(result);
                        result = NULL;
												return result;
                    }
                    else
                    {
                        Base64decode(B64BUFFER_u_char(pt_buffer1), source);
												return pt_buffer1;
                    }
                }
            }
        }
    }
  return result;  
}
//extern U8 Task_section1;


STRING_HANDLE Base64_Encode_Internal(const unsigned char* source, size_t size)
{
    STRING_HANDLE result =0;
    size_t neededSize = 0;
    char encoded[800];
    size_t currentPosition = 0;
		size_t destinationPosition = 0;
    neededSize += (size == 0) ? (0) : ((((size - 1) / 3) + 1) * 4);
    neededSize += 1; /*+1 because \0 at the end of the string*/
		encoded_string_size = neededSize;			//by megha on 2/2017	
    /*Codes_SRS_BASE64_06_006: [If when allocating memory to produce the encoding a failure occurs then Base64_Encode shall return NULL.]*/
//     encoded = (char*)malloc(neededSize);
//     if (encoded == NULL)
//     {
//         result = NULL;
//        // LogError("Base64_Encode:: Allocation failed.");
//     }
//     else
//     {
        /*b0            b1(+1)          b2(+2)
        7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
        |----c1---| |----c2---| |----c3---| |----c4---|
        */

        
        while (size - currentPosition >= 3)
        {
            char c1 = base64char(source[currentPosition] >> 2);
            char c2 = base64char(
                ((source[currentPosition] & 3) << 4) |
                    (source[currentPosition + 1] >> 4)
            );
            char c3 = base64char(
                ((source[currentPosition + 1] & 0x0F) << 2) |
                    ((source[currentPosition + 2] >> 6) & 3)
            );
            char c4 = base64char(
                source[currentPosition + 2] & 0x3F
            );
            currentPosition += 3;
            encoded[destinationPosition++] = c1;
            encoded[destinationPosition++] = c2;
            encoded[destinationPosition++] = c3;
            encoded[destinationPosition++] = c4;

        }
        if (size - currentPosition == 2)
        {
            char c1 = base64char(source[currentPosition] >> 2);
            char c2 = base64char(
                ((source[currentPosition] & 0x03) << 4) |
                    (source[currentPosition + 1] >> 4)
            );
            char c3 = base64b16(source[currentPosition + 1] & 0x0F);
            encoded[destinationPosition++] = c1;
            encoded[destinationPosition++] = c2;
            encoded[destinationPosition++] = c3;
            encoded[destinationPosition++] = '=';
        }
        else if (size - currentPosition == 1)
        {
            char c1 = base64char(source[currentPosition] >> 2);
            char c2 = base64b8(source[currentPosition] & 0x03);
            encoded[destinationPosition++] = c1;
            encoded[destinationPosition++] = c2;
            encoded[destinationPosition++] = '=';
            encoded[destinationPosition++] = '=';
        }
				//Task_section1=60;

        /*null terminating the string*/
        encoded[destinationPosition] = '\0';
        /*Codes_SRS_BASE64_06_007: [Otherwise Base64_Encode shall return a pointer to STRING, that string contains the base 64 encoding of input.]*/
//         result = B64STRING_new_with_memory(encoded);
//         if (result == NULL)
//         {
//             free(encoded);
// 						return (result);
// 							
//            // LogError("Base64_Encode:: Allocation failed for return value.");
//         }
// 				else
// 				{
						//decoded_buf = malloc(encoded_string_size);
				 // memset(&periodic_data_copy1,'\0',sizeof(periodic_data_copy1)); 
				memset(decoded_buf,'\0',sizeof(decoded_buf));
				 //Task_section1=63;
						memcpy(decoded_buf,encoded,encoded_string_size);
				 //Task_section1=64;
// 						memset(pt_String1->s,'\0',encoded_string_size+2);
//  						memcpy(pt_String1->s	,decoded_buf,encoded_string_size);
						


// 					
// 				}
//    }
			return result;
}

STRING_HANDLE Base64_Encode_Bytes(const unsigned char* source, size_t size)
{
    STRING_HANDLE result;
    /*Codes_SRS_BASE64_02_001: [If source is NULL then Base64_Encode_Bytes shall return NULL.] */
    if (source == NULL)
    {
        result = NULL;
    }
    /*Codes_SRS_BASE64_02_002: [If source is not NULL and size is zero, then Base64_Encode_Bytes shall produce an empty STRING_HANDLE.] */
    else if (size == 0)
    {
        result = B64STRING_new(); /*empty string*/
				return result;
    }
    else
    {
        result = Base64_Encode_Internal(source, size);
				pt_String1 = result;
		    return pt_String1;

    }
		return result;
}

STRING_HANDLE Base64_Encode(BUFFER_HANDLE input)
{
    STRING_HANDLE result;
    /*the following will happen*/
    /*1. the "data" of the binary shall be "eaten" 3 characters at a time and produce 4 base64 encoded characters for as long as there are more than 3 characters still to process*/
    /*2. the remaining characters (1 or 2) shall be encoded.*/
    /*there's a level of assumption that 'a' corresponds to 0b000000 and that '_' corresponds to 0b111111*/
    /*the encoding will use the optional [=] or [==] at the end of the encoded string, so that other less standard aware libraries can do their work*/
    /*these are the bits of the 3 normal bytes to be encoded*/

    /*Codes_SRS_BASE64_06_001: [If input is NULL then Base64_Encode shall return NULL.]*/
    if (input == NULL)
    {
        result = NULL;
       // LogError("Base64_Encode:: NULL input");
    }
    else
    {
        size_t inputSize;
        const unsigned char* inputBinary;
        if ((B64BUFFER_content(input, &inputBinary) != 0) ||
            (B64BUFFER_size(input, &inputSize) != 0))
        {
            result = NULL;
           // LogError("Base64_Encode:: BUFFER_routines failure.");
        }
        else
        {
            result = Base64_Encode_Internal(inputBinary, inputSize);
        }
    }
    return result;
}

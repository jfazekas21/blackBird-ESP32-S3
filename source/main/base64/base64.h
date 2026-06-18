// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/** @file base64.h
*	@brief Prototypes for functions related to encoding/decoding
*	a @c buffer using standard base64 encoding.
*/
#include <stdint.h>
#include <stddef.h>
#ifndef BASE64_H
#define BASE64_H
typedef struct STRING_TAG
{
    char* s;
}B64STRING;
typedef struct STRING_TAG* STRING_HANDLE;
typedef struct BUFFER_TAG
{
    unsigned char* buffer;
    size_t size;
}BUFFER;
void Base64decode(unsigned char *decodedString, const char *base64String);
BUFFER* Base64_Decoder(const char* source);
B64STRING* Base64_Encode(BUFFER* input);
extern int encoded_string_size,decoded_buf_size;
//extern  struct STRING_TAG* STRING_HANDLE;
extern B64STRING* pt_String1 ;
extern BUFFER * pt_buffer1 ;
//extern char *decoded_buf;// __attribute__ ((section ("STRUCTURES_STACK")));
extern  char decoded_buf[800];
STRING_HANDLE Base64_Encode_Internal(const unsigned char* source, size_t size);
#endif /* BASE64_H */

//
//  wrapper_aes.h
//  xcode_aes_only_c
//
//  Created by Mob on 2021/3/1.
//  Copyright © 2021年 leo. All rights reserved.
//
#ifndef wrapper_aes_h
#define wrapper_aes_h

#include "dstr.h"
#ifdef __cplusplus
extern "C"
{
#endif
    EXPORT void encode_aes_with_key_wrapper(char* content, char * aes_key, struct dstr *output);
    EXPORT void decode_aes_with_key_wrapper(char* content, char * aes_key, struct dstr *output);

    EXPORT void encode_aes_wrapper(char* content, struct dstr *output);
    EXPORT void decode_aes_wrapper(char* content, struct dstr *output);
#ifdef __cplusplus
}
#endif

#endif /* wrapper_aes_h */

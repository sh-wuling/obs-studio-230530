//
//  wrapper_aes.c
//  xcode_aes_only_c
//
//  Created by Mob on 2021/3/1.
//  Copyright © 2021年 leo. All rights reserved.
//

#include "wrapper_aes.h"
#include <cstring>
#include "AES.hpp"
#include "base64.hpp"
#ifdef __cplusplus
extern "C"
{
#endif

void encode_aes_with_key_wrapper(char *in, char *aes_key, struct dstr *output) {
    size_t in_len = strlen((const char *) in);
    unsigned int out_len_aes = 0;
    AES aes(128);
    unsigned char *encode = aes.EncryptECB((unsigned char *) in, (unsigned int) in_len, (unsigned char *) aes_key, out_len_aes);
    string base64 = base64_encode(encode, out_len_aes);
    dstr_cat(output, base64.c_str());
}

void decode_aes_with_key_wrapper(char *content, char *aes_key, struct dstr *output) {
    string in = base64_decode(content);
    auto in_len = in.length();
    AES aes(128);
    const char *result = (const char *) aes.DecryptECB((unsigned char *) in.c_str(), (unsigned int) in_len, (unsigned char *) aes_key);
    dstr_cat(output, result);
}

void encode_aes_wrapper(char* content, struct dstr *output){
    char key[] = "obs_live_0000000";
    encode_aes_with_key_wrapper(content, key, output);
}

void decode_aes_wrapper(char *content, struct dstr *output) {
    if (content == nullptr) {
        return;
    }
    char key[] = "obs_live_0000000";
    decode_aes_with_key_wrapper(content, key, output);
}

#ifdef __cplusplus
}
#endif

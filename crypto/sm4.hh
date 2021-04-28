#pragma once
#ifndef _SM4_HH_
#define _SM4_HH_

#include<string>

#define SM4_ENCRYPT 1
#define SM4_DECRYPT 0

std::string
decrypt_SM4_EBC(const std::string &ctext, const std::string &raw_key);

std::string
encrypt_SM4_EBC(const std::string &ptext, const std::string &raw_key);

typedef struct
{
    int mode;             /*!<  encrypt/decrypt   */
    unsigned long sk[32]; /*!<  SM4 subkeys       */
} sm4_context;

#ifdef __cplusplus
extern "C"
{
#endif

    /**   
     * \brief          SM4 key schedule (128-bit, encryption)   
     *   
     * \param ctx      SM4 context to be initialized   
     * \param key      16-byte secret key   
     */
    void sm4_setkey_enc(sm4_context *ctx, unsigned char key[16]);

    /**   
     * \brief          SM4 key schedule (128-bit, decryption)   
     *   
     * \param ctx      SM4 context to be initialized   
     * \param key      16-byte secret key   
     */
    void sm4_setkey_dec(sm4_context *ctx, unsigned char key[16]);

    /**   
     * \brief          SM4-ECB block encryption/decryption   
     * \param ctx      SM4 context   
     * \param mode     SM4_ENCRYPT or SM4_DECRYPT   
     * \param length   length of the input data   
     * \param input    input block   
     * \param output   output block   
     */
    void sm4_crypt_ecb(sm4_context *ctx,
                       int mode,
                       int length,
                       unsigned char *input,
                       unsigned char *output);

    /**
     * Not yet implemented.
     */
    void sm4_crypt_cbc(sm4_context *ctx,
                      int mode,
                      int length,
                      unsigned char IV[16],
                      unsigned char *input,
                      unsigned char *output
                      );
#ifdef __cplusplus
}
#endif

#endif

#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <cstring>

#include "crypto.h"
#include "crypto_scrypt.h"
#include "bip38.h"

#define PASSFACTOR_SIZE 32
#define PASSPHRASE_MAGIC_SIZE 8
#define PASSPHRASE_SIZE (PASSPHRASE_MAGIC_SIZE + OWNERSALT_SIZE + 33)
#define DERIVED_SIZE 64
#define ADDRESSHASH_SIZE 4
#define OWNERSALT_SIZE 8

void print_hex(char * hex, size_t len) {
    int i;
    for(i=0; i<len; i++) {
        printf("%.02x",(unsigned char)hex[i]);
    }
}

std::string encode_base16(std::vector<unsigned char> data, size_t len)
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (std::vector<unsigned char>::iterator it = data.begin();  it != data.end(); ++it) {
        ss << std::setw(2) << static_cast<int>(*it);
    }
    return ss.str();
}

std::vector<unsigned char> decode_base16(const std::string &hex)
{
    std::vector<unsigned char> bytes;

    for (unsigned int i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        char byte = (char) strtol(byteString.c_str(), NULL, 16);
        bytes.push_back(byte);
    }

    return bytes;
}

std::vector<unsigned char> decrypt_bip38_ec(const std::vector<unsigned char> key,  const std::string& passwd)
{
    int i;
    uint8_t passfactor[PASSFACTOR_SIZE];

    memset(passfactor,0,PASSFACTOR_SIZE);

    const unsigned char * s_key = reinterpret_cast<const unsigned char*>(key.data());

    crypto_scrypt((const uint8_t *)passwd.c_str(), passwd.length(),
                   &s_key[3 + ADDRESSHASH_SIZE], OWNERSALT_SIZE,
                   16384, 8, 8, passfactor, PASSFACTOR_SIZE );

    // compute EC point (passpoint) using passfactor
    struct bp_key ec_point;
    if(!bp_key_init(&ec_point)) {
        fprintf(stderr,"%s","cannot init EC point key");
        exit(3);
    }
    if(!bp_key_secret_set(&ec_point,passfactor,PASSFACTOR_SIZE)) {
        fprintf(stderr,"%s","cannot set EC point from passfactor");
        exit(3);
    }

    // get the passpoint as bytes
    unsigned char * passpoint;
    size_t passpoint_len;

    if(!bp_pubkey_get(&ec_point,(unsigned char **)&passpoint,&passpoint_len)) {
        fprintf(stderr,"%s","cannot get pubkey for EC point");
        exit(4);
    }

    // now we need to decrypt seedb
    uint8_t encryptedpart2[16];
    memset(encryptedpart2,0,16);
    memcpy(encryptedpart2, &s_key[3 + ADDRESSHASH_SIZE + OWNERSALT_SIZE + 8], 16);

    uint8_t encryptedpart1[16];
    memset(encryptedpart1,0,16);
    memcpy(encryptedpart1, &s_key[3 + ADDRESSHASH_SIZE + OWNERSALT_SIZE], 8);

    unsigned char derived[DERIVED_SIZE];
    // get the encryption key for seedb using scrypt
    // with passpoint as the key, salt is addresshash+ownersalt
    unsigned char derived_scrypt_salt[ADDRESSHASH_SIZE + OWNERSALT_SIZE];
    memcpy(derived_scrypt_salt, &s_key[3], ADDRESSHASH_SIZE); // copy the addresshash
    memcpy(derived_scrypt_salt+ADDRESSHASH_SIZE, &s_key[3+ADDRESSHASH_SIZE], OWNERSALT_SIZE); // copy the ownersalt
    crypto_scrypt( passpoint, passpoint_len,
                   derived_scrypt_salt, ADDRESSHASH_SIZE+OWNERSALT_SIZE,
                   1024, 1, 1, derived, DERIVED_SIZE );

    //get decryption key
    unsigned char derivedhalf2[DERIVED_SIZE/2];
    memcpy(derivedhalf2, derived+(DERIVED_SIZE/2), DERIVED_SIZE/2);

    unsigned char iv[32];
    memset(iv,0,32);

    EVP_CIPHER_CTX d;
    EVP_CIPHER_CTX_init(&d);
    EVP_DecryptInit_ex(&d, EVP_aes_256_ecb(), NULL, derivedhalf2, iv);

    unsigned char unencryptedpart2[32];
    int decrypt_len;

    EVP_DecryptUpdate(&d, unencryptedpart2, &decrypt_len, encryptedpart2, 16);
    EVP_DecryptUpdate(&d, unencryptedpart2, &decrypt_len, encryptedpart2, 16);
    for(i=0; i<16; i++) {
        unencryptedpart2[i] ^= derived[i + 16];
    }
    unsigned char unencryptedpart1[32];
    memcpy(encryptedpart1+8, unencryptedpart2, 8);
    EVP_DecryptUpdate(&d, unencryptedpart1, &decrypt_len, encryptedpart1, 16);
    EVP_DecryptUpdate(&d, unencryptedpart1, &decrypt_len, encryptedpart1, 16);
    for(i=0; i<16; i++) {
        unencryptedpart1[i] ^= derived[i];
    }

    // recoved seedb
    unsigned char seedb[24];
    memcpy(seedb, unencryptedpart1, 16);
    memcpy(&(seedb[16]), &(unencryptedpart2[8]), 8);

    // turn seedb into factorb (factorb = SHA256(SHA256(seedb)))
    unsigned char factorb[32];
    bu_Hash(factorb, seedb, 24);

    // multiply by passfactor (ec_point_pub)
    const EC_GROUP * ec_group = EC_KEY_get0_group(ec_point.k);
    const EC_POINT * ec_point_pub = EC_KEY_get0_public_key(ec_point.k);
    BIGNUM * bn_passfactor = BN_bin2bn(passfactor,32,BN_new());
    BIGNUM * bn_factorb = BN_bin2bn(factorb,32,BN_new());
    BIGNUM * bn_res = BN_new();
    BIGNUM * bn_final = BN_new();
    BIGNUM * bn_n = BN_new();
    BN_CTX * ctx = BN_CTX_new();
    EC_GROUP_get_order(ec_group, bn_n, ctx);
    BN_mul(bn_res, bn_passfactor, bn_factorb, ctx);
    BN_mod(bn_final, bn_res, bn_n, ctx);

    unsigned char finalKey[32];
    memset(finalKey, 0, 32);
    int n = BN_bn2bin(bn_final, finalKey);

    BN_clear_free(bn_passfactor);
    BN_clear_free(bn_factorb);
    BN_clear_free(bn_res);
    BN_clear_free(bn_n);
    BN_clear_free(bn_final);

    printf("\n");
    print_hex((char *)finalKey, 32);
    printf("\n");

    std::vector<unsigned char> out;
    out.assign(finalKey, finalKey + 32);

    return out;
}

std::vector<unsigned char> decrypt_bip38(const std::vector<unsigned char> enc_data,  const std::string& passwd)
{
	//def decrypt(encrypted_privkey, passphrase, p):
	//
	//#1. Collect encrypted private key and passphrase from user.
	//#	passed as parameters
    const unsigned char *data = reinterpret_cast<const unsigned char*>(enc_data.data());

    unsigned char key[64];
    unsigned char addresshash[4];
    unsigned char encryptedhalf1[16], encryptedhalf2[16];
    unsigned char decryptedhalf1[16], decryptedhalf2[16];
    unsigned char derivedhalf1[32], derivedhalf2[32];

	memcpy(addresshash, &data[3], 4);
	memcpy(encryptedhalf1, &data[7], 16);
	memcpy(encryptedhalf2, &data[23], 16);

	//#3. Derive decryption key for seedb using scrypt with passpoint, addresshash, and ownersalt
	//key = scrypt.hash(passphrase, addresshash, 16384, 8, p)
    crypto_scrypt((const uint8_t *)passwd.c_str(), passwd.length(),
                   &addresshash[0], ADDRESSHASH_SIZE, 16384, 8, 8, key, 64);

	memcpy(derivedhalf1, &key[0], 32);
	memcpy(derivedhalf2, &key[32], 32);
	//
	//#4. Decrypt encryptedpart2 using AES256Decrypt to yield the last 8 bytes of seedb and the last 8 bytes of encryptedpart1.
	//Aes = aes.Aes(derivedhalf2)
	//decryptedhalf2 = Aes.dec(encryptedhalf2)

    int decrypt_len;
    EVP_CIPHER_CTX de;

    EVP_CIPHER_CTX_init(&de);
    EVP_DecryptInit_ex(&de, EVP_aes_256_cbc(), NULL, derivedhalf2, NULL);
    EVP_DecryptUpdate(&de, decryptedhalf2, &decrypt_len, encryptedhalf2, 16);

	//#5. Decrypt encryptedpart1 to yield the remainder of seedb.
	//decryptedhalf1 = Aes.dec(encryptedhalf1)
    EVP_DecryptInit_ex(&de, EVP_aes_256_cbc(), NULL, derivedhalf2, NULL);
    EVP_DecryptUpdate(&de, decryptedhalf1, &decrypt_len, encryptedhalf1, 16);

	//priv = decryptedhalf1 + decryptedhalf2
	//priv = binascii.unhexlify('%064x' % (long(binascii.hexlify(priv), 16) ^ long(binascii.hexlify(derivedhalf1), 16)))
	//return priv, addresshash

    unsigned char priv[32];
    memcpy(priv, decryptedhalf1, 16);
    memcpy(priv + 16, decryptedhalf2, 16);

    for (int i = 0; i < 32; i++) {
    	priv[i] ^= derivedhalf1[i];
    }

//    printf("\n");
//    print_hex((char *)priv, 32);
//    printf("\n");

    std::vector<unsigned char> out;
    out.assign(priv, priv + 32);

    return out;
}

std::vector<unsigned char> encrypt_bip38(const std::vector<unsigned char> priv_key,
		const std::string& address, const std::string& passwd)
{
    unsigned char key[64];
    unsigned char derivedhalf1[32], derivedhalf2[32];
    unsigned char encryptedhalf1[16], encryptedhalf2[16];
    unsigned char part1[32], part2[32];

    const unsigned char * p_address = reinterpret_cast<const unsigned char*>(address.data());
    const unsigned char * p_secret = reinterpret_cast<const unsigned char*>(priv_key.data());

	// 1. take the first four bytes of SHA256(SHA256()) of it. Let's call this "addresshash".
    //		addresshash = hashlib.sha256(hashlib.sha256(address).digest()).digest()[:4]  # salt
    unsigned char addresshash[32];
    bu_Hash(addresshash, &p_address[0], 34);

    //		#2. Derive a key from the passphrase using scrypt
    //		#	 a.  Parameters: passphrase is the passphrase itself encoded in UTF-8.
    //		#		 addresshash came from the earlier step, n=16384, r=8, p=8, length=64
    //		#		 (n, r, p are provisional and subject to consensus)
    //		key = scrypt.hash(passphrase, addresshash, 16384, 8, p)
    crypto_scrypt((const uint8_t *)passwd.c_str(), passwd.length(),
                   &addresshash[0], ADDRESSHASH_SIZE, 16384, 8, 8, key, 64);

    memcpy(derivedhalf1, &key[0], 32);
    memcpy(derivedhalf2, &key[32], 32);

    //		#3. Do AES256Encrypt(bitcoinprivkey[0...15] xor derivedhalf1[0...15], derivedhalf2), call the 16-byte result encryptedhalf1
    //		Aes = aes.Aes(derivedhalf2)
    //		encryptedhalf1 = Aes.enc(enc.sxor(privK[:16], derivedhalf1[:16]))
    //

	for (int i=0; i < 16; i++)
		part1[i] = p_secret[i] ^ derivedhalf1[i];

	for (int i=0; i < 16; i++)
		part2[i] = p_secret[i + 16] ^ derivedhalf1[i + 16];

    int encrypt_len;
    EVP_CIPHER_CTX en;

    EVP_CIPHER_CTX_init(&en);
    EVP_EncryptInit_ex(&en, EVP_aes_256_cbc(), NULL, derivedhalf2, NULL);
    EVP_EncryptUpdate(&en, encryptedhalf1, &encrypt_len, part1, 16);
    EVP_EncryptInit_ex(&en, EVP_aes_256_cbc(), NULL, derivedhalf2, NULL);
    EVP_EncryptUpdate(&en, encryptedhalf2, &encrypt_len, part2, 16);

	//		#5. The encrypted private key is the Base58Check-encoded concatenation of the following, which totals 39 bytes without Base58 checksum:
	//		#		0x01 0x42 + flagbyte + salt + encryptedhalf1 + encryptedhalf2
	//		flagbyte = chr(0b11100000)  # 11 no-ec 1 compressed-pub 00 future 0 ec only 00 future
	//		privkey = ('\x01\x42' + flagbyte + addresshash + encryptedhalf1 + encryptedhalf2)
	//		check = hashlib.sha256(hashlib.sha256(privkey).digest()).digest()[:4]
	//		return enc.b58encode(privkey + check)

	unsigned char flagbyte = 0xc0;// 0b11100000; // 11 no-ec 1 compressed-pub 00 future 0 ec only 00 future
	unsigned char pref1 = 0x01;
	unsigned char pref2 = 0x42;

	unsigned char enc_key[128];
	enc_key[0] = pref1;
	enc_key[1] = pref2;
	enc_key[2] = flagbyte;

	memcpy(enc_key + 3, addresshash, ADDRESSHASH_SIZE);
	memcpy(enc_key + 3 + ADDRESSHASH_SIZE, encryptedhalf1, 16);
	memcpy(enc_key + 3 + ADDRESSHASH_SIZE + 16, encryptedhalf2, 16);

	return std::vector<unsigned char>(&enc_key[0], &enc_key[3 + ADDRESSHASH_SIZE + 32]);
}

#include <openssl/ec.h>
#include <openssl/evp.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bp_key {
	EC_KEY		*k;
};

extern void bu_Hash(unsigned char *md256, const unsigned char *data, size_t data_len);
extern bool bp_key_init(struct bp_key *key);
extern void bp_key_free(struct bp_key *key);
extern bool bp_key_generate(struct bp_key *key);
extern bool bp_privkey_set(struct bp_key *key, const unsigned char *privkey, size_t pk_len);
extern bool bp_pubkey_set(struct bp_key *key, const unsigned char *pubkey, size_t pk_len);
extern bool bp_key_secret_set(struct bp_key *key, const unsigned char *privkey_, size_t pk_len);
extern bool bp_privkey_get(const struct bp_key *key, unsigned char **privkey, size_t *pk_len);
extern bool bp_pubkey_get(const struct bp_key *key, unsigned char **pubkey, size_t *pk_len);
extern bool bp_key_secret_get(unsigned char *p, size_t len, const struct bp_key *key);
extern bool bp_sign(const struct bp_key *key, const unsigned char *data, size_t data_len,
		unsigned char **sig_, size_t *sig_len_);
extern bool bp_verify(const struct bp_key *key, const unsigned char *data, size_t data_len,
		unsigned char *sig, size_t sig_len);

#ifdef __cplusplus
}
#endif

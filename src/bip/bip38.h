/*
 * bip38.h
 *
 *  Created on: 30 дек. 2015 г.
 *      Author: mac
 */

#ifndef SRC_BIP_BIP38_H_
#define SRC_BIP_BIP38_H_

#include <string>

std::vector<unsigned char> encrypt_bip38(const std::vector<unsigned char> key, const std::string& address, const std::string& passwd);
std::vector<unsigned char> decrypt_bip38(const std::vector<unsigned char> key,  const std::string& passwd);
std::vector<unsigned char> decrypt_bip38_ec(const std::vector<unsigned char> key,  const std::string& passwd);
std::string encode_base16(std::vector<unsigned char> data, size_t len);
std::vector<unsigned char> decode_base16(const std::string &hex);

#endif /* SRC_BIP_BIP38_H_ */

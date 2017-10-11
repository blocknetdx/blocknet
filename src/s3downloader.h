#ifndef S3DOWNLOADER_H
#define S3DOWNLOADER_H

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>


/**
  * @brief downloadBlackList
  * @param host - host, from download
  * @param get - input file
  * @param skip - counter of header
  * @return true, if success
  */
bool downloadBlackList(std::list<std::string> blackList, const std::string &host = "dxlist.blocknet.co", const std::string &get = "/txlist.txt");


#endif // S3DOWNLOADER_H

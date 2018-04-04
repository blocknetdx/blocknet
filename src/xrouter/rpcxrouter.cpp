#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <exception>
#include <iostream>

#include "xrouterapp.h"
#include "uint256.h"
using namespace json_spirit;

//******************************************************************************
//******************************************************************************
Value xrGetBlocks(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlocks\nLookup blocks in a specified blockchain.");
    }

    uint256 id = uint256();

    auto statusCode = xrouter::App::instance().getBlocks(id);
    if (statusCode == xrouter::SUCCESS) {
        return id.GetHex();
    } else {
        Object error;
        error.emplace_back(json_spirit::Pair("error", ""));
        error.emplace_back(json_spirit::Pair("code", statusCode));
        error.emplace_back(json_spirit::Pair("name", __FUNCTION__));
        return error;
    }
}

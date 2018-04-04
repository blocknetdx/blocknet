#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <exception>

using namespace json_spirit;

//******************************************************************************
//******************************************************************************
Value xrGetBlocks(const Array & params, bool fHelp)
{
    if (fHelp) {
        throw std::runtime_error("xrGetBlocks\nLookup blocks in a specified blockchain.");
    }
    Array result;
    return result;
}

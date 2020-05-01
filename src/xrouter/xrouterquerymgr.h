// Copyright (c) 2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XROUTER_XROUTERQUERYMGR_H
#define BLOCKNET_XROUTER_XROUTERQUERYMGR_H

#include <hash.h>
#include <sync.h>
#include <uint256.h>
#include <univalue.h>
#include <xrouter/xrouterutils.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>

namespace xrouter {

class QueryMgr {
public:
    typedef std::string QueryReply;
    typedef std::pair<std::shared_ptr<boost::mutex>, std::shared_ptr<boost::condition_variable> > QueryCondition;

    explicit QueryMgr() = default;

    /**
     * Add a query. This stores interal state including condition variables and associated mutexes.
     * @param id uuid of query, can't be empty
     * @param node address of node associated with query, can't be empty
     */
    void addQuery(const std::string & id, const NodeAddr & node);

    /**
     * Store a query reply.
     * @param id
     * @param node
     * @param reply
     * @return Total number of replies for the query with specified id.
     */
    int addReply(const std::string & id, const NodeAddr & node, const std::string & reply);

    /**
     * Fetch a reply. This method returns the number of matching replies.
     * @param id
     * @param reply
     * @return
     */
    int reply(const std::string & id, const NodeAddr & node, std::string & reply);

    /**
     * Fetch the most common reply for a specific query. If a group of nodes return results and 2 of 3 are
     * matching, this will return the most common reply, i.e. the replies of the matching two.
     * @param id
     * @param reply Most common reply
     * @param replies All replies
     * @param agree Set of nodes that provided most common replies
     * @param diff Set of nodes that provided non-common replies
     * @return
     */
    int mostCommonReply(const std::string & id, std::string & reply, std::map<NodeAddr, std::string> & replies,
                        std::set<NodeAddr> & agree, std::set<NodeAddr> & diff);

    /**
     * Fetch the most common reply for a specific query. If a group of nodes return results and 2 of 3 are
     * matching, this will return the most common reply, i.e. the replies of the matching two.
     * @param id
     * @param reply Most common reply
     * @return
     */
    int mostCommonReply(const std::string & id, std::string & reply);

    /**
     * Returns true if the query with specified id.
     * @param id
     * @return
     */
    bool hasQuery(const std::string & id);

    /**
     * Returns true if the query with specified id and node address is valid.
     * @param id
     * @param node
     * @return
     */
    bool hasQuery(const std::string & id, const NodeAddr & node);

    /**
     * Returns true if a query for the specified node exists.
     * @param node
     * @return
     */
    bool hasNodeQuery(const NodeAddr & node);

    /**
     * Returns true if the reply exists for the specified node.
     * @param id
     * @param nodeAddr node address
     * @return
     */
    bool hasReply(const std::string & id, const NodeAddr & node);

    /**
     * Returns the query's mutex.
     * @param id
     * @param node
     * @return
     */
    std::shared_ptr<boost::mutex> queryLock(const std::string & id, const NodeAddr & node);

    /**
     * Returns the queries condition variable.
     * @param id
     * @param node
     * @return
     */
    std::shared_ptr<boost::condition_variable> queryCond(const std::string & id, const NodeAddr & node);

    /**
     * Return all replies associated with a query.
     * @param id
     * @return
     */
    std::map<std::string, QueryReply> allReplies(const std::string & id);

    /**
     * Return all query locks associated with an id.
     * @param id
     * @return
     */
    std::map<std::string, QueryCondition> allLocks(const std::string & id);

    /**
     * Purges the ephemeral state of a query with specified id.
     * @param id
     */
    void purge(const std::string & id);

    /**
     * Purges the ephemeral state of a query with specified id and node address.
     * @param id
     * @param node
     */
    void purge(const std::string & id, const NodeAddr & node);

    /**
     * Return time of the last request to specified node.
     * @param node
     * @param command
     * @return
     */
    std::chrono::time_point<std::chrono::system_clock> getLastRequest(const NodeAddr & node, const std::string & command);

    /**
     * Returns true if a request to a node has been made previously.
     * @param node
     * @param command
     * @return
     */
    bool hasSentRequest(const NodeAddr & node, const std::string & command);

    /**
     * Updates (or adds) a request time for the specified node and command.
     * @param node
     * @param command
     */
    void updateSentRequest(const NodeAddr & node, const std::string & command);

    /**
     * Returns true if the rate limit has been exceeded on requests to the specified node.
     * @param node Node address
     * @param service Name of the command or service
     * @param lastRequest Time of last request
     * @param rateLimit Rate limit in milliseconds
     * @return
     */
    bool rateLimitExceeded(const NodeAddr & node, const std::string & service,
            std::chrono::time_point<std::chrono::system_clock> lastRequest, int rateLimit);

    /**
     * Return snode score for specified node address.
     * @param node
     * @return
     */
    int getScore(const NodeAddr & node);

    /**
     * Returns true if the snode has an associated score.
     * @param node
     * @return
     */
    bool hasScore(const NodeAddr & node);

    /**
     * Updates the snode score.
     * @param node
     * @param score
     * @return Snode's overall score
     */
    int updateScore(const NodeAddr & node, int score);

    /**
     * Ban the snode and set the default score when ban expires.
     * @param node
     * @return
     */
    int banScore(const NodeAddr & node);

private:
    static bool hasError(const std::string & reply);

private:
    Mutex mu;
    std::map<std::string, std::map<NodeAddr, QueryCondition> > queriesLocks;
    std::map<std::string, std::map<NodeAddr, QueryReply> > queries;
    std::map<NodeAddr, std::map<std::string, std::chrono::time_point<std::chrono::system_clock> > > queriesLastSent;
    std::unordered_map<NodeAddr, int> snodeScore;
};

}

#endif //BLOCKNET_XROUTER_XROUTERQUERYMGR_H

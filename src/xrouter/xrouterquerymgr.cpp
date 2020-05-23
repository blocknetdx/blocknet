// Copyright (c) 2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <xrouter/xrouterquerymgr.h>

namespace xrouter {

void QueryMgr::addQuery(const std::string & id, const NodeAddr & node) {
    if (id.empty() || node.empty())
        return;

    LOCK(mu);

    if (!queries.count(id))
        queries[id] = std::map<NodeAddr, std::string>{};

    auto m = std::make_shared<boost::mutex>();
    auto cond = std::make_shared<boost::condition_variable>();

    if (!queriesLocks.count(id))
        queriesLocks[id] = std::map<NodeAddr, QueryCondition>{};

    auto qc = QueryCondition{m, cond};
    queriesLocks[id][node] = qc;
}

int QueryMgr::addReply(const std::string & id, const NodeAddr & node, const std::string & reply) {
    if (id.empty() || node.empty())
        return 0;

    int replies{0};
    QueryCondition qcond;

    {
        LOCK(mu);

        if (!queries.count(id))
            return 0; // done, no query found with id

        // Total replies
        replies = queriesLocks.count(id);
        // Query condition
        if (replies)
            qcond = queriesLocks[id][node];
        // If invalid query condition return
        if (!qcond.first || !qcond.second)
            return 0;
    }

    if (replies) { // only handle locks if they exist for this query
        boost::mutex::scoped_lock l(*qcond.first);
        queries[id][node] = reply; // Assign reply
        qcond.second->notify_all();
    }

    LOCK(mu);
    return queries.count(id);
}

int QueryMgr::reply(const std::string & id, const NodeAddr & node, std::string & reply) {
    LOCK(mu);

    int consensus = queries.count(id);
    if (!consensus)
        return 0;

    reply = queries[id][node];
    return consensus;
}

int QueryMgr::mostCommonReply(const std::string & id, std::string & reply, std::map<NodeAddr, std::string> & replies,
        std::set<NodeAddr> & agree, std::set<NodeAddr> & diff)
{
    LOCK(mu);

    int consensus = queries.count(id);
    if (!consensus || queries[id].empty())
        return 0;

    // all replies
    replies = queries[id];

    std::map<uint256, std::string> hashes;
    std::map<uint256, int> counts;
    std::map<uint256, std::set<NodeAddr> > nodes;
    for (auto & item : queries[id]) {
        auto result = item.second;
        try {
            UniValue j;
            if (j.read(result)) {
                if (j.isObject() || j.isArray())
                    result = j.write();
                else
                    result = j.getValStr();
            }
        } catch (...) {
            result = item.second;
        }
        auto hash = Hash(result.begin(), result.end());
        hashes[hash] = item.second;
        counts[hash] += 1; // update counts for common replies
        nodes[hash].insert(item.first);
    }

    // sort reply counts descending (most similar replies are more valuable)
    std::vector<std::pair<uint256, int> > tmp(counts.begin(), counts.end());
    std::sort(tmp.begin(), tmp.end(),
              [](const std::pair<uint256, int> & a, const std::pair<uint256, int> & b) {
                  return a.second > b.second;
              });

    diff.clear();
    if (tmp.size() > 1) {
        if (tmp[0].second == tmp[1].second) { // Check for errors and re-sort if there's a tie and highest rank has error
            const auto &r = hashes[tmp[0].first];
            if (hasError(r)) { // in tie arrangements we don't want errors to take precendence
                std::sort(tmp.begin(), tmp.end(), // sort descending
                          [this, &hashes](const std::pair<uint256, int> & a, const std::pair<uint256, int> & b) {
                              const auto & ae = hasError(hashes[a.first]);
                              const auto & be = hasError(hashes[b.first]);
                              if ((!ae && !be) || (ae && be))
                                  return a.second > b.second;
                              return be;
                          });
            }
        }
        // Filter nodes that responded with different results
        for (int i = 1; i < static_cast<int>(tmp.size()); ++i) {
            const auto & hash = tmp[i].first;
            if (!nodes.count(hash) || tmp[i].second >= tmp[0].second) // do not penalize equal counts, only fewer
                continue;
            auto ns = nodes[hash];
            diff.insert(ns.begin(), ns.end());
        }
    }

    auto selhash = tmp[0].first;

    // store agreeing nodes
    agree.clear();
    agree = nodes[selhash];

    // select the most common replies
    reply = hashes[selhash];
    return tmp[0].second;
}

int QueryMgr::mostCommonReply(const std::string & id, std::string & reply) {
    std::map<NodeAddr, std::string> replies;
    std::set<NodeAddr> agree;
    std::set<NodeAddr> diff;
    return mostCommonReply(id, reply, replies, agree, diff);
}

bool QueryMgr::hasQuery(const std::string & id) {
    LOCK(mu);
    return queriesLocks.count(id);
}

bool QueryMgr::hasQuery(const std::string & id, const NodeAddr & node) {
    LOCK(mu);
    return queriesLocks.count(id) && queriesLocks[id].count(node);
}

bool QueryMgr::hasNodeQuery(const NodeAddr & node) {
    LOCK(mu);
    for (const auto & item : queriesLocks) {
        if (item.second.count(node))
            return true;
    }
    return false;
}

bool QueryMgr::hasReply(const std::string & id, const NodeAddr & node) {
    LOCK(mu);
    return queries.count(id) && queries[id].count(node);
}

std::shared_ptr<boost::mutex> QueryMgr::queryLock(const std::string & id, const NodeAddr & node) {
    LOCK(mu);
    if (!queriesLocks.count(id))
        return nullptr;
    if (!queriesLocks[id].count(node))
        return nullptr;
    return queriesLocks[id][node].first;
}

std::shared_ptr<boost::condition_variable> QueryMgr::queryCond(const std::string & id, const NodeAddr & node) {
    LOCK(mu);
    if (!queriesLocks.count(id))
        return nullptr;
    if (!queriesLocks[id].count(node))
        return nullptr;
    return queriesLocks[id][node].second;
}

std::map<std::string, QueryMgr::QueryReply> QueryMgr::allReplies(const std::string & id) {
    LOCK(mu);
    return queries[id];
}

std::map<std::string, QueryMgr::QueryCondition> QueryMgr::allLocks(const std::string & id) {
    LOCK(mu);
    return queriesLocks[id];
}

void QueryMgr::purge(const std::string & id) {
    LOCK(mu);
    queriesLocks.erase(id);
}

void QueryMgr::purge(const std::string & id, const NodeAddr & node) {
    LOCK(mu);
    if (queriesLocks.count(id))
        queriesLocks[id].erase(node);
}

std::chrono::time_point<std::chrono::system_clock> QueryMgr::getLastRequest(const NodeAddr & node, const std::string & command) {
    LOCK(mu);
    if (queriesLastSent.count(node) && queriesLastSent[node].count(command))
        return queriesLastSent[node][command];
    return std::chrono::system_clock::from_time_t(0);
}

bool QueryMgr::hasSentRequest(const NodeAddr & node, const std::string & command) {
    LOCK(mu);
    return queriesLastSent.count(node) && queriesLastSent[node].count(command);
}

void QueryMgr::updateSentRequest(const NodeAddr & node, const std::string & command) {
    LOCK(mu);
    queriesLastSent[node][command] = std::chrono::system_clock::now();
}

bool QueryMgr::rateLimitExceeded(const NodeAddr & node, const std::string & service,
        const std::chrono::time_point<std::chrono::system_clock> lastRequest, const int rateLimit)
{
    if (rateLimit <= 0)
        return false; // rate limiting disabled
    if (hasSentRequest(node, service)) {
        std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();
        std::chrono::system_clock::duration diff = time - lastRequest;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() < std::chrono::milliseconds(rateLimit).count())
            return true;
    }
    return false;
}

int QueryMgr::getScore(const NodeAddr & node) {
    if (hasScore(node)) {
        LOCK(mu);
        return snodeScore[node];
    }
    return 0;
}

bool QueryMgr::hasScore(const NodeAddr & node) {
    LOCK(mu);
    return snodeScore.count(node);
}

int QueryMgr::updateScore(const NodeAddr & node, const int score) {
    LOCK(mu);
    if (!snodeScore.count(node))
        snodeScore[node] = 0;
    snodeScore[node] += score;
    return snodeScore[node];
}

int QueryMgr::banScore(const NodeAddr & node) {
    LOCK(mu);
    snodeScore[node] = -30;
    return snodeScore[node];
}

//private static
bool QueryMgr::hasError(const std::string & reply) {
    UniValue uv;
    if (!uv.read(reply) || !uv.isObject())
        return false;
    const auto & err_v = find_value(uv, "error");
    return !err_v.isNull();
}

}

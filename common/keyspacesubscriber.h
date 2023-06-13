#pragma once

#include <string>
#include <set>
#include <memory>
#include <deque>
#include "redisselect.h"
#include "dbconnector.h"

namespace swss {

/* 
    Use for subscribing to multiple patterns and to be used as selectable 
    TODO: Don't limit the scope to a DB
*/
class KeySpaceSubscriber : public RedisSelect
{
public:
    KeySpaceSubscriber(const std::string &dbName, int pri = 0, const std::string& clientName = "");

    KeySpaceSubscriber(DBConnector *db, int pri = 0, const std::string& clientName = "");

    /* Use the existing redisContext, SELECT DB and SUBSCRIBE */
    void subscribe(const std::string &pattern);

    /* PSUBSCRIBE */
    void psubscribe(const std::string &pattern);

    /* PUNSUBSCRIBE */
    void punsubscribe(const std::string &pattern);

    std::deque<std::string> pops();

    /* Read keyspace event from redis */
    uint64_t readData() override;

    bool hasData() override;
    
    uint64_t getMaxEvents() override {return static_cast<uint64_t>(m_patterns.size());}

private:
    /* Pop keyspace event from event buffer. Caller should free resources. */
    std::shared_ptr<RedisReply> popEventBuffer();

    std::deque<std::shared_ptr<RedisReply>> m_keyspace_event_buffer;

    std::set<std::string> m_patterns;

    void setClientName(const std::string& name)
    {
        if (m_subscribe && !name.empty())
        {
            m_subscribe->setClientName(name);
        }
    }
};

}
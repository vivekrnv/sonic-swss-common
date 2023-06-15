#pragma once

#include <string>
#include <set>
#include <memory>
#include <deque>
#include "redisselect.h"
#include "dbconnector.h"

namespace swss {

/**
 * @brief A Redis Selectable that can subscribe to multiple patterns
 *
 * Note: All the Patters must be limited to a DB
*/
class KeySpaceSubscriber : public RedisSelect
{
public:
    KeySpaceSubscriber(DBConnector *parentConn, int pri = 0);

    /* Use the existing redisContext, SELECT DB and SUBSCRIBE */
    void subscribe(const std::string &);

    /* PSUBSCRIBE */
    void psubscribe(const std::string &);

    /* PUNSUBSCRIBE */
    void punsubscribe(const std::string &);

    /* Bulk Subscribe */
    void subscribe(const std::vector<std::string> &);
    void psubscribe(const std::vector<std::string> &);
    void punsubscribe(const std::vector<std::string> &);

    /* Return all the keyspace notifications seen */
    void pops(std::deque<RedisMessage> &dQ);

    /* Read keyspace event from redis */
    uint64_t readData() override;

    bool hasData() override {return m_keyspace_event_buffer.size() > 0;};
    
    uint64_t getMaxEvents() override {return static_cast<uint64_t>(m_patterns.size());}

    void setClientName(const std::string& name);

protected:
    /* Pop keyspace event from event buffer */
    bool popEventBuffer(RedisMessage& msg);

    std::deque<std::shared_ptr<RedisReply>> m_keyspace_event_buffer;

    std::set<std::string> m_patterns;

private:
    std::string processBulk(const std::vector<std::string>&, bool);
};

}
#pragma once

#include <string>
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
    KeySpaceSubscriber(const std::string &dbName, int pri = 0);

    /* Use the existing redisContext, SELECT DB and SUBSCRIBE */
    void subscribe(const std::string &pattern);

    /* PSUBSCRIBE */
    void psubscribe(const std::string &pattern);

    /* PUNSUBSCRIBE */
    void punsubscribe(const std::string &pattern);

    std::deque<std::string> pops();

    /* Read keyspace event from redis */
    uint64_t readData() override;

private:
    /* Pop keyspace event from event buffer. Caller should free resources. */
    std::shared_ptr<RedisReply> popEventBuffer();

    std::deque<std::shared_ptr<RedisReply>> m_keyspace_event_buffer;
};

}
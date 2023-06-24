#pragma once

#include <string>
#include <set>
#include <memory>
#include <deque>
#include "redisselect.h"
#include "dbconnector.h"

namespace swss {

/**
 * @brief A Redis Selectable that can subscribe to multiple channels with a single redis context
 *
 * Note: All the Patterns must be limited to a DB
*/
class RedisMCSelect : public RedisSelect
{
public:
    RedisMCSelect(DBConnector *parentConn, int pri, const std::string& name = "RedisMCSelect");

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

    uint64_t getMaxEvents() override {return static_cast<uint64_t>(m_patterns.size());}

    /* Read reply event from redis */
    uint64_t readData() override;

    bool hasData() override {return m_event_buffer.size() > 0;}
    bool hasCachedData() override {return m_event_buffer.size() > 1;}

protected:
    std::deque<std::shared_ptr<RedisReply>> m_event_buffer;
    std::set<std::string> m_patterns;

private:
    std::string processBulk(const std::vector<std::string>&, bool);
};


/**
 * @brief RedisMCSelect implementation that should be used by the 
 * consumers which operate on KeySpace Notifications
*/
class MultiKeySpaceSubscriber : public RedisMCSelect
{
public:
    MultiKeySpaceSubscriber(DBConnector *parentConn, int pri = 0, const std::string& name = "MultiKeySpaceSubscriber") :
        RedisMCSelect(parentConn, pri, name) {}
    
    /* XLate the RedisReply stored in the event buffer into a Redis Message */
    void pops(std::deque<RedisMessage> &dQ);

protected:
    bool popEventBuffer(RedisMessage& msg);
};

/**
 * TODO: Add a RedisMCSelect implementation that should be used by the 
 * consumers which operate on Notification Channels
*/
/* class MultiNotificationConsumer : public RedisMCSelect {}; */
}
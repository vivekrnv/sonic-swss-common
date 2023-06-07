from swsscommon.swsscommon import KeySpaceSubscriber, Select
import syslog
import signal
import sys

class MockHostConfigDaemon:
    def __init__(self):
        self.key_space = KeySpaceSubscriber("CONFIG_DB")

    def subscribe(self, table):
        m_keyspace = f"__keyspace@4__:{table}|*"
        print(f"Subscribed to {m_keyspace}")
        self.key_space.psubscribe(m_keyspace)

    def subscribe_tables(self):
        self.subscribe('KDUMP')
        # Handle FEATURE updates before other tables
        self.subscribe('FEATURE')
        # Handle AAA and TACACS related tables
        self.subscribe('AAA')
        self.subscribe('TACPLUS_SERVER')
        self.subscribe('TACPLUS')
        self.subscribe('LOOPBACK_INTERFACE') 
        # Handle NTP & NTP_SERVER updates
        self.subscribe('NTP_SERVER')
        self.subscribe('NTP')
        self.subscribe('AUTO_TECHSUPPORT')
        self.subscribe('PORT')

    def start(self):
        self.selector = Select()
        self.selector.addSelectable(self.key_space)
        while True:
            state, selectable_ = self.selector.select(2000)
            if state == self.selector.TIMEOUT:
                print("error returned by timeout")
                continue
            elif state == self.selector.ERROR:
                print("error returned by select")
                continue
            print(self.key_space.pops())
            print("Select Returned by selev")

def signal_handler(sig, frame):
    if sig == signal.SIGHUP:
        syslog.syslog(syslog.LOG_INFO, "HostCfgd: signal 'SIGHUP' is caught and ignoring..")
    elif sig == signal.SIGINT:
        syslog.syslog(syslog.LOG_INFO, "HostCfgd: signal 'SIGINT' is caught and exiting...")
        sys.exit(128 + sig)
    elif sig == signal.SIGTERM:
        syslog.syslog(syslog.LOG_INFO, "HostCfgd: signal 'SIGTERM' is caught and exiting...")
        sys.exit(128 + sig)
    else:
        syslog.syslog(syslog.LOG_INFO, "HostCfgd: invalid signal - ignoring..")

def main():
    daemon = MockHostConfigDaemon()
    daemon.subscribe_tables()
    daemon.start()

if __name__ == "__main__":
    main()
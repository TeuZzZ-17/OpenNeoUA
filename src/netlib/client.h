#ifndef ZNDNET_CLIENT_H_INCLUDED
#define ZNDNET_CLIENT_H_INCLUDED

#include <vector>
#include <string>
#include <atomic>

namespace ZNDNet
{

class ZNDClient: public ZNDNet
{
public:
    ZNDClient(const std::string &servstring);

    //Client methods
    void Start(const std::string &name, const IPaddress &addr);
    uint8_t GetStatus();

    bool GetSessions(SessionInfoVect &dst);
    void CreateSession(const std::string &name, const std::string &pass, uint32_t max_players);
    void JoinSession(uint64_t SID, const std::string &pass);
    void SendDisconnect();

    void CloseSession(uint32_t closeTime) override;
    void KickUser(uint64_t _ID) override;

    bool GetUsers(UserInfoVect &dst) override;
    bool GetUser(UserInfo &dst, const char *name) override;
    bool GetUser(UserInfo &dst, uint64_t _ID) override;

    void SendData(uint64_t to, void *data, uint32_t sz, uint8_t flags = 0, uint8_t channel = CHANNEL_USR) override;
    void BroadcastData(void *data, uint32_t sz, uint8_t flags = 0, uint8_t channel = CHANNEL_USR) override;

    void ShowSession(bool show) override;

    uint64_t GetMyID() override;
    std::string GetMyName() override;

    uint64_t GetMySesID() override;

protected:

    Pkt *Recv_PreparePacket(InRawPkt *pkt);

    void ProcessSystemPkt(Pkt *pkt);
    void ProcessRegularPkt(Pkt *pkt);

    void SendConnect();
    void RequestGamesList();

    void InterprocessUpdate();

    static int _RecvThread(void *data);
    static int _SendThread(void *data);
    static int _UpdateThread(void *data);
// Data
public:

protected:

    std::atomic_uint_fast32_t eStatus;

    IPaddress   cServAddress;
    bool        cServHasLobby;
    NetUser     cME;
    bool        cLeader;
    std::string cJoinedSessionName;
    uint64_t    cTimeOut;

    SessionInfoVect cSessions;
    uint64_t        cSessionsReqTimeNext;
    std::atomic_bool cSessionsMakeRequest;

    UserInfoVect    cUsers;
};

}

#endif

#include "NetManager.h"
#include <mongo/client/dbclient.h>
CNetManager::CNetManager()
{
	CMessageDispatcher::getRef().RegisterSessionMessage(ID_DT2OS_DirectServerAuth,
		std::bind(&CNetManager::msg_ConnectServerAuth, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

	//ע���¼�
	CMessageDispatcher::getRef().RegisterOnSessionEstablished(std::bind(&CNetManager::event_OnSessionEstablished, this, std::placeholders::_1, std::placeholders::_1));
	CMessageDispatcher::getRef().RegisterOnSessionDisconnect(std::bind(&CNetManager::event_OnSessionDisconnect, this, std::placeholders::_1, std::placeholders::_1));
}

bool CNetManager::Start()
{
	m_configListen.aID = 1;
	m_configListen.listenIP = GlobalFacade::getRef().getServerConfig().getConfigListen(AuthNode).ip;
	m_configListen.listenPort = GlobalFacade::getRef().getServerConfig().getConfigListen(AuthNode).port;
	m_configListen.maxSessions = 100;

	if (CTcpSessionManager::getRef().AddAcceptor(m_configListen) == InvalidAccepterID)
	{
		LOGE("AddAcceptor Failed. listenIP=" << m_configListen.listenIP << ", listenPort=" << m_configListen.listenPort);
		return false;
	}
	LOGI("CNetManager Init Success.");
	return true;
}

void CNetManager::event_OnSessionEstablished(AccepterID aID, SessionID sID)
{
	WriteStreamPack ws;
	ProtoDirectServerAuth auth;
	auth.srcNode = GlobalFacade::getRef().getServerConfig().getOwnServerNode();
	auth.srcIndex = GlobalFacade::getRef().getServerConfig().getOwnNodeIndex();
	ws << ID_DT2OS_DirectServerAuth << auth;
	CTcpSessionManager::getRef().SendOrgSessionData(aID, sID, ws.GetStream(), ws.GetStreamLen());
}

void CNetManager::event_OnSessionDisconnect(AccepterID aID, SessionID sID)
{
	LOGW("event_OnSessionDisconnect sID=" << sID);
	auto founder = std::find_if(m_onlineAgent.begin(), m_onlineAgent.end(),
		[sID](const ServerAuthSession & sac){ return sac.sID == sID; });
	if (founder == m_onlineAgent.end())
	{
		LOGW("event_OnSessionDisconnect not found the sID=" << sID);
		return;
	}
	m_onlineAgent.erase(founder);
}


void CNetManager::msg_ConnectServerAuth(AccepterID aID, SessionID sID, ProtocolID pID, ReadStreamPack & rs)
{
	ProtoDirectServerAuth auth;
	rs >> auth;
	ServerAuthSession sac;
	sac.aID = aID;
	sac.sID = sID;
	sac.node = auth.srcNode;
	sac.index = auth.srcIndex;

	auto founder = std::find_if(m_onlineAgent.begin(), m_onlineAgent.end(),
		[auth](const ServerAuthSession & cas){return cas.index == auth.srcIndex; });

	if (founder != m_onlineAgent.end())
	{
		CTcpSessionManager::getRef().KickSession(founder->aID, founder->sID);
		m_onlineAgent.erase(founder);
	}
	m_onlineAgent.push_back(sac);
	LOGI("msg_ServerInit agent sID=" << sID << ", index=" << auth.srcIndex);
}



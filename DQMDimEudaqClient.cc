/*
 *
 * DQMDimEventCollector.cc source template automatically generated by a class generator
 * Creation date : mer. sept. 9 2015
 *
 * This file is part of DQM4HEP libraries.
 * 
 * DQM4HEP is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * based upon these libraries are permitted. Any copy of these libraries
 * must include this copyright notice.
 * 
 * DQM4HEP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with DQM4HEP.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * @author Remi Ete
 * @copyright CNRS , IPNL
 */

// -- dqm4hep headers
#include "dqm4hep/DQMDimEventCollector.h"
#include "dqm4hep/DQMEvent.h"
#include "dqm4hep/DQMEventStreamer.h"
#include "dqm4hep/DQMLogging.h"
#include "dqm4hep/DQMCoreTool.h"

namespace dqm4hep
{

static const char DQMDimEventCollector_emptyBuffer [] = "EMPTY";
static const uint32_t DQMDimEventCollector_emptyBufferSize = 5;

DimEventRequestRpc::DimEventRequestRpc(DQMDimEventCollector *pCollector) :
	DimRpc((char*)("DQM4HEP/EventCollector/" + pCollector->getCollectorName() + "/EVENT_RAW_REQUEST").c_str(), "C", "C"),
	m_pCollector(pCollector)
{
	/* nop */
}

//-------------------------------------------------------------------------------------------------

void DimEventRequestRpc::rpcHandler()
{
	m_pCollector->handleEventRequest(this);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

DQMDimEudaqClient::DQMDimEudaqClient() :
		m_eudaqName("DEFAULT"),
		m_streamerName("DEFAULT"),
		m_isRunning(false),

		m_pCollectEventCommand(NULL),  // Not sure about these
		m_pEventRequestRpc(NULL),      // Not sure about these
		m_pUpdateModeCommand(NULL),    // Not sure about these
		m_pEventUpdateService(NULL),   // Not sure about these
		m_pEventStreamer(NULL),        // Not sure about these

		m_pCurrentEvent(NULL),
		m_state(0),
		m_clientRegisteredId(0),
		m_pBuffer(0),
		m_pSubEventBuffer(0)
{
	DimServer::addClientExitHandler(this);

	// write only buffer with an initial size of 4 Mo (should be enough to start)
	m_pSubEventBuffer = new xdrstream::BufferDevice(4*1024*1024);
}

//-------------------------------------------------------------------------------------------------

DQMDimEudaqClient::~DQMDimEudaqClient() :
{
	if(isRunning())
		stopCollector();

	if(m_pEventStreamer)
		delete m_pEventStreamer;

	if(m_pCurrentEvent)
		delete m_pCurrentEvent;

	if(m_pBuffer)
		delete m_pBuffer;

	delete m_pSubEventBuffer;
}

bool DQMDimEudaqClient::isRunning() const
{
	return m_isRunning;
}

StatusCode DQMDimEudaqClient::setCollectorName(const std::string &collectorName)
{
	if(isRunning())
		return STATUS_CODE_NOT_ALLOWED;

	m_collectorName = collectorName;

	return STATUS_CODE_SUCCESS;
}

const std::string & DQMDimEudaqClient::getCollectorName() const
{
	return m_collectorName;
}

void DQMDimEudaqClient::setEventStreamer(DQMEventStreamer *pEventStreamer)
{
	if(m_pEventStreamer)
		delete m_pEventStreamer;

	m_pEventStreamer = pEventStreamer;
}

DQMEventStreamer *DQMDimEudaqClient::getEventStreamer() const
{
	return m_pEventStreamer;
}

StatusCode DQMDimEudaqClient::startCollector()
{
	if(this->isRunning())
		return STATUS_CODE_SUCCESS;

	m_pEventRequestRpc = new DimEventRequestRpc(this);

	m_pUpdateModeCommand = new DimCommand(("DQM4HEP/EventCollector/" + getCollectorName() + "/UPDATE_MODE").c_str(), "I", this);
	m_pCollectEventCommand = new DimCommand(("DQM4HEP/EventCollector/" + getCollectorName() + "/COLLECT_RAW_EVENT").c_str(), "C", this);
	m_pSubEventIdentifierCommand = new DimCommand(("DQM4HEP/EventCollector/" + getCollectorName() + "/SUB_EVENT_IDENTIFIER").c_str(), "C", this);
	m_pClientRegitrationCommand = new DimCommand(("DQM4HEP/EventCollector/" + getCollectorName() + "/CLIENT_REGISTRATION").c_str(), "I", this);

	m_pEventUpdateService = new DimService(("DQM4HEP/EventCollector/" + getCollectorName() + "/EVENT_RAW_UPDATE").c_str(), "C",
			(void*) &DQMDimEventCollector_emptyBuffer[0], DQMDimEventCollector_emptyBufferSize);

	m_pStatisticsService = new DQMStatisticsService("DQM4HEP/EventCollector/" + getCollectorName() + "/STATS");
	m_pClientRegisteredService = new DimService(("DQM4HEP/EventCollector/" + getCollectorName() + "/CLIENT_REGISTERED").c_str(), m_clientRegisteredId);
	m_pServerStateService = new DimService(("DQM4HEP/EventCollector/" + getCollectorName() + "/SERVER_STATE").c_str(), m_state);

	// inform clients that the server is available for registrations
	LOG4CXX_INFO( dqmMainLogger , "Changing server application to running !" );

	DimServer::start( ("DQM4HEP/EventCollector/" + getCollectorName()).c_str() );

	// time needed for registration on dns node
	DQMCoreTool::sleep(std::chrono::seconds(1));

	m_state = 1;
	m_pServerStateService->updateService(m_state);

	m_isRunning = true;

	return STATUS_CODE_SUCCESS; 
}

StatusCode DQMDimEudaqClient::stopCollector() 
{
	if(!this->isRunning())
		return STATUS_CODE_SUCCESS;

	m_state = 0;
	// inform clients that the server is shut down
	m_pServerStateService->updateService(m_state);

	delete m_pCollectEventCommand;
	delete m_pUpdateModeCommand;
	delete m_pSubEventIdentifierCommand;
	delete m_pClientRegitrationCommand;

	delete m_pEventUpdateService;
	delete m_pStatisticsService;
	delete m_pClientRegisteredService;
	delete m_pServerStateService;

	delete m_pEventRequestRpc;

	m_pBuffer->reset();


	m_isRunning = false;

	return STATUS_CODE_SUCCESS;
}

void DQMDimEudaqClient::handleEventReception(DimCommand *pDimCommand)
{
	dqm_char *pBuffer = static_cast<char*>(pDimCommand->getData());
	dqm_uint bufferSize = pDimCommand->getSize();

	if(NULL == pBuffer || 0 == bufferSize)
		return;

	m_pStatisticsService->update(bufferSize);
	DQMEvent *pEvent = NULL;

	try
	{
		xdrstream::BufferDevice *pDevice = this->configureBuffer(pBuffer, bufferSize);

		// if streamer is available, de-serialize the event
		if(NULL != m_pEventStreamer)
		{
			THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pEventStreamer->read(pEvent, pDevice));

			// replace the current event
			if(NULL != pEvent)
			{
				if(NULL != m_pCurrentEvent)
					delete m_pCurrentEvent;

				m_pCurrentEvent = pEvent;
			}
		}
	}
	catch(StatusCodeException &exception)
	{
		if(NULL != pEvent)
			delete pEvent;

		LOG4CXX_ERROR( dqmMainLogger , "Couldn't deserialize the buffer : " << exception.getStatusCode() );
		return;
	}

	LOG4CXX_DEBUG( dqmMainLogger , "Event received" );

	this->updateEventService();
}

                            //----------//

//   Everything above here is done, everything below is from the source, and still needs to be edited   //

                            //----------//

void DQMDimEventCollector::handleEventRequest(DimEventRequestRpc *pDimRpc)
{
	char *pSubEventIdentifier = pDimRpc->getString();
	std::string subEventIdentifier;

	if(NULL != pSubEventIdentifier)
		subEventIdentifier = pSubEventIdentifier;

	if(NULL != m_pEventStreamer && NULL != m_pCurrentEvent && !subEventIdentifier.empty())
	{
		try
		{
			m_pSubEventBuffer->reset();
			THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pEventStreamer->write(m_pCurrentEvent, subEventIdentifier, m_pSubEventBuffer));

			dqm_char *pEventBuffer = m_pSubEventBuffer->getBuffer();
			int bufferSize = m_pSubEventBuffer->getPosition();

			pDimRpc->setData((void *) pEventBuffer, bufferSize);
		}
		catch(...)
		{
		}
	}
	else if(NULL != m_pBuffer && NULL != m_pBuffer->getBuffer() && 0 != m_pBuffer->getPosition())
		pDimRpc->setData((void *) m_pBuffer->getBuffer(), m_pBuffer->getPosition());
	else
		pDimRpc->setData((void *) &DQMDimEventCollector_emptyBuffer[0], DQMDimEventCollector_emptyBufferSize);
}

DQMDimEventCollector::Client &DQMDimEventCollector::getClient(int clientId)
{
	ClientMap::iterator findIter = m_clientMap.find(clientId);

	if(m_clientMap.end() != findIter)
		return findIter->second;

	Client newClient;
	newClient.m_clientId = clientId;
	newClient.m_updateMode = false;
	newClient.m_subEventIdentifier = "";

	m_clientMap.insert(std::pair<int, Client>(clientId, newClient));

	return m_clientMap.find(clientId)->second;
}

void DQMDimEventCollector::commandHandler()
{
	DimCommand *pCommand = getCommand();

	if(!pCommand)
		return;

	if(pCommand == m_pUpdateModeCommand)
	{
		bool updateMode = static_cast<bool>(pCommand->getInt());
		int clientId = getClientId();

		if(clientId < 0)
			return;

		Client &client = getClient(clientId);
		client.m_updateMode = updateMode;
		return;
	}

	if(pCommand == m_pSubEventIdentifierCommand)
	{
		char *pSubEventIdentifier = pCommand->getString();
		int clientId = getClientId();
		std::string subEventIdentifier;

		if(NULL != pSubEventIdentifier)
			subEventIdentifier = pSubEventIdentifier;

		if(clientId < 0)
			return;

		Client &client = getClient(clientId);
		client.m_subEventIdentifier = subEventIdentifier;
		return;
	}

	if(pCommand == m_pCollectEventCommand)
	{
		this->handleEventReception(pCommand);
		return;
	}

	if(pCommand == m_pClientRegitrationCommand)
	{
		int clientId = getClientId();

		if(clientId < 0)
			return;

		int registerClient = pCommand->getInt();

		if(registerClient)
		{
			Client &client = getClient(clientId);
			LOG4CXX_INFO( dqmMainLogger , "Client " << clientId << " added to server !" );

			int clientIds[2];
			clientIds[0] = clientId;
			clientIds[1] = 0;

			m_clientRegisteredId = clientId;
			m_pClientRegisteredService->selectiveUpdateService(m_clientRegisteredId, &clientIds[0]);
			m_clientRegisteredId = 0;
		}
		else
		{
			this->removeClient(clientId);
		}
	}
}

//-------------------------------------------------------------------------------------------------

void DQMDimEventCollector::clientExitHandler()
{
	this->removeClient(getClientId());
}

//-------------------------------------------------------------------------------------------------

void DQMDimEventCollector::updateEventService()
{
	// if not running, do not update
	if(!isRunning())
		return;

	// event available ?
	if(NULL == m_pBuffer)
	{
		LOG4CXX_DEBUG( dqmMainLogger , "Buffer device is null" );
		return;
	}

	if(NULL == m_pBuffer->getBuffer())
	{
		LOG4CXX_DEBUG( dqmMainLogger , "Buffer is null" );
		return;
	}

	if( 0 == m_pBuffer->getBufferSize() )
	{
		LOG4CXX_DEBUG( dqmMainLogger , "Buffer position is 0" );
		return;
	}

	int *clientIds = new int[m_clientMap.size()];
	memset(&clientIds[0], 0, m_clientMap.size());
	int currentId = 0;

	for(ClientMap::iterator iter = m_clientMap.begin(), endIter = m_clientMap.end() ;
			endIter != iter ; ++iter)
	{
		// check for update mode
		if(!iter->second.m_updateMode)
			continue;

		// specific case where the client has queried a sub part of the event
		if(!iter->second.m_subEventIdentifier.empty()
		&& (NULL != m_pEventStreamer && NULL != m_pCurrentEvent))
		{
			// try to serialize the sub event
			m_pSubEventBuffer->reset();

			if(STATUS_CODE_SUCCESS != m_pEventStreamer->write(m_pCurrentEvent, iter->second.m_subEventIdentifier, m_pSubEventBuffer))
			{
				LOG4CXX_WARN( dqmMainLogger , "Couldn't write event (sub event serialization)" );
				continue;
			}

			dqm_char *pEventBuffer = m_pSubEventBuffer->getBuffer();
			int bufferSize = m_pSubEventBuffer->getPosition();

			if(NULL == pEventBuffer || 0 == bufferSize)
				continue;

			int clientIdArray[2];
			clientIdArray[0] = iter->first;
			clientIdArray[1] = 0;

			m_pEventUpdateService->selectiveUpdateService((void *) pEventBuffer, bufferSize, &clientIdArray[0]);

			continue;
		}

		clientIds[currentId] = iter->first;
		currentId ++;
	}

	if(currentId != 0)
	{
		LOG4CXX_DEBUG( dqmMainLogger , "Sending updates to " << currentId << " clients !" );
		m_pEventUpdateService->selectiveUpdateService((void *) m_pBuffer->getBuffer(), m_pBuffer->getBufferSize(), clientIds);
	}

	delete [] clientIds;

	return;
}

//-------------------------------------------------------------------------------------------------

void DQMDimEventCollector::removeClient(int clientId)
{
	if(clientId < 0)
		return;

	ClientMap::iterator findIter = m_clientMap.find(clientId);

	if(m_clientMap.end() == findIter)
		return;

	m_clientMap.erase(findIter);

	LOG4CXX_INFO( dqmMainLogger , "Client " << clientId << " removed from server !" );
}

//-------------------------------------------------------------------------------------------------

xdrstream::BufferDevice *DQMDimEventCollector::configureBuffer(char *pBuffer, xdrstream::xdr_size_t bufferSize)
{
	// allocate it in read only mode and set ownership to false
	if(0 == m_pBuffer)
		// false for no copy
		m_pBuffer = new xdrstream::BufferDevice(pBuffer, bufferSize, true);
	else
		m_pBuffer->setBuffer(pBuffer, bufferSize, true);

	m_pBuffer->setOwner(true);

	return m_pBuffer;
}

} 

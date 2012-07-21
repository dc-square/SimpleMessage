/*******************************************************************************
 * Copyright (c) 2009, 2012 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/

/**
 * @file
 * \brief Functions dealing with the MQTT protocol exchanges
 *
 * Some other related functions are in the MQTTProtocolOut module
 * */


#include <stdlib.h>

#include "MQTTProtocolClient.h"
#if !defined(NO_PERSISTENCE)
#include "MQTTPersistence.h"
#endif
#include "SocketBuffer.h"
#include "StackTrace.h"
#include "Heap.h"

#if !defined(min)
#define min(A,B) ( (A) < (B) ? (A):(B))
#endif

void Protocol_processPublication(Publish* publish, Clients* client);
void MQTTProtocol_closeSession(Clients* client, int sendwill);

extern MQTTProtocol state;
extern ClientStates* bstate;

/**
 * List callback function for comparing Message structures by message id
 * @param a first integer value
 * @param b second integer value
 * @return boolean indicating whether a and b are equal
 */
int messageIDCompare(void* a, void* b)
{
	Messages* msg = (Messages*)a;
	return msg->msgid == *(int*)b;
}


/**
 * Assign a new message id for a client.  Make sure it isn't already being used and does
 * not exceed the maximum.
 * @param client a client structure
 * @return the next message id to use
 */
int MQTTProtocol_assignMsgId(Clients* client)
{
	FUNC_ENTRY;
	++(client->msgID);
	while (ListFindItem(client->outboundMsgs, &(client->msgID), messageIDCompare) != NULL)
		++(client->msgID);
	if (client->msgID == MAX_MSG_ID + 1)
		client->msgID = 1;
	FUNC_EXIT_RC(client->msgID);
	return client->msgID;
}


void MQTTProtocol_storeQoS0(Clients* pubclient, Publish* publish)
{
	int len;
	pending_write* pw = NULL;

	FUNC_ENTRY;
	/* store the publication until the write is finished */
	pw = malloc(sizeof(pending_write));
	Log(TRACE_MIN, 12, NULL);
	pw->p = MQTTProtocol_storePublication(publish, &len);
	pw->socket = pubclient->socket;
	ListAppend(&(state.pending_writes), pw, sizeof(pending_write)+len);
	/* we don't copy QoS 0 messages unless we have to, so now we have to tell the socket buffer where
	the saved copy is */
	if (SocketBuffer_updateWrite(pw->socket, pw->p->topic, pw->p->payload) == NULL)
		Log(LOG_SEVERE, 0, "Error updating write");
	FUNC_EXIT;
}


/**
 * Utility function to start a new publish exchange.
 * @param pubclient the client to send the publication to
 * @param publish the publication data
 * @param qos the MQTT QoS to use
 * @param retained boolean - whether to set the MQTT retained flag
 * @return the completion code
 */
int MQTTProtocol_startPublishCommon(Clients* pubclient, Publish* publish, int qos, int retained)
{
	int rc = TCPSOCKET_COMPLETE;

	FUNC_ENTRY;
	rc = MQTTPacket_send_publish(publish, 0, qos, retained, pubclient->socket, pubclient->clientID);
	if (qos == 0 && rc == TCPSOCKET_INTERRUPTED)
		MQTTProtocol_storeQoS0(pubclient, publish);
	FUNC_EXIT_RC(rc);
	return rc;
}


/**
 * Start a new publish exchange.  Store any state necessary and try to send the packet
 * @param pubclient the client to send the publication to
 * @param publish the publication data
 * @param qos the MQTT QoS to use
 * @param retained boolean - whether to set the MQTT retained flag
 * @param mm - pointer to the message to send
 * @return the completion code
 */
int MQTTProtocol_startPublish(Clients* pubclient, Publish* publish, int qos, int retained, Messages** mm)
{
	Publish p = *publish;
	int rc = 0;

	FUNC_ENTRY;
	if (qos > 0)
	{
		p.msgId = publish->msgId = MQTTProtocol_assignMsgId(pubclient);
		*mm = MQTTProtocol_createMessage(publish, mm, qos, retained);
		ListAppend(pubclient->outboundMsgs, *mm, (*mm)->len);
		/* we change these pointers to the saved message location just in case the packet could not be written
		entirely; the socket buffer will use these locations to finish writing the packet */
		p.payload = (*mm)->publish->payload;
		p.topic = (*mm)->publish->topic;
	}
	rc = MQTTProtocol_startPublishCommon(pubclient, &p, qos, retained);
	FUNC_EXIT_RC(rc);
	return rc;
}


/**
 * Copy and store message data for retries
 * @param publish the publication data
 * @param mm - pointer to the message data to store
 * @param qos the MQTT QoS to use
 * @param retained boolean - whether to set the MQTT retained flag
 * @return pointer to the message data stored
 */
Messages* MQTTProtocol_createMessage(Publish* publish, Messages **mm, int qos, int retained)
{
	Messages* m = malloc(sizeof(Messages));

	FUNC_ENTRY;
	m->len = sizeof(Messages);
	if (*mm == NULL || (*mm)->publish == NULL)
	{
		int len1;
		*mm = m;
		m->publish = MQTTProtocol_storePublication(publish, &len1);
		m->len += len1;
	}
	else
	{
		++(((*mm)->publish)->refcount);
		m->publish = (*mm)->publish;
	}
	m->msgid = publish->msgId;
	m->qos = qos;
	m->retain = retained;
	time(&(m->lastTouch));
	if (qos == 2)
		m->nextMessageType = PUBREC;
	FUNC_EXIT;
	return m;
}


/**
 * Store message data for possible retry
 * @param publish the publication data
 * @param len returned length of the data stored
 * @return the publication stored
 */
Publications* MQTTProtocol_storePublication(Publish* publish, int* len)
{
	Publications* p = malloc(sizeof(Publications));

	FUNC_ENTRY;
	p->refcount = 1;

	*len = strlen(publish->topic)+1;
	if (Heap_findItem(publish->topic))
		p->topic = publish->topic;
	else
	{
		p->topic = malloc(*len);
		strcpy(p->topic, publish->topic);
	}
	*len += sizeof(Publications);

	p->topiclen = publish->topiclen;
	p->payloadlen = publish->payloadlen;
	p->payload = malloc(publish->payloadlen);
	memcpy(p->payload, publish->payload, p->payloadlen);
	*len += publish->payloadlen;

	ListAppend(&(state.publications), p, *len);
	FUNC_EXIT;
	return p;
}

/**
 * Remove stored message data.  Opposite of storePublication
 * @param p stored publication to remove
 */
void MQTTProtocol_removePublication(Publications* p)
{
	FUNC_ENTRY;
	if (--(p->refcount) == 0)
	{
		free(p->payload);
		free(p->topic);
		ListRemove(&(state.publications), p);
	}
	FUNC_EXIT;
}

/**
 * Process an incoming publish packet for a socket
 * @param pack pointer to the publish packet
 * @param sock the socket on which the packet was received
 * @return completion code
 */
int MQTTProtocol_handlePublishes(void* pack, int sock)
{
	Publish* publish = (Publish*)pack;
	Clients* client = NULL;
	char* clientid = NULL;
	int rc = TCPSOCKET_COMPLETE;

	FUNC_ENTRY;
	client = (Clients*)(ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
	clientid = client->clientID;
	Log(LOG_PROTOCOL, 11, NULL, sock, clientid, publish->msgId, publish->header.bits.qos,
					publish->header.bits.retain, min(20, publish->payloadlen), publish->payload);

	if (publish->header.bits.qos == 0)
		Protocol_processPublication(publish, client);
	else if (publish->header.bits.qos == 1)
	{
		/* send puback before processing the publications because a lot of return publications could fill up the socket buffer */
		rc = MQTTPacket_send_puback(publish->msgId, sock, client->clientID);
		/* if we get a socket error from sending the puback, should we ignore the publication? */
		Protocol_processPublication(publish, client);
	}
	else if (publish->header.bits.qos == 2)
	{
		/* store publication in inbound list */
		int len;
		ListElement* listElem = NULL;
		Messages* m = malloc(sizeof(Messages));
		Publications* p = MQTTProtocol_storePublication(publish, &len);
		m->publish = p;
		m->msgid = publish->msgId;
		m->qos = publish->header.bits.qos;
		m->retain = publish->header.bits.retain;
		m->nextMessageType = PUBREL;
		if ( ( listElem = ListFindItem(client->inboundMsgs, &(m->msgid), messageIDCompare) ) != NULL )
		{   /* discard queued publication with same msgID that the current incoming message */
			Messages* msg = (Messages*)(listElem->content);
			MQTTProtocol_removePublication(msg->publish);
			ListInsert(client->inboundMsgs, m, sizeof(Messages) + len, listElem);
			ListRemove(client->inboundMsgs, msg);
		} else
			ListAppend(client->inboundMsgs, m, sizeof(Messages) + len);
		rc = MQTTPacket_send_pubrec(publish->msgId, sock, client->clientID);
		publish->topic = NULL;
	}
	MQTTPacket_freePublish(publish);
	FUNC_EXIT_RC(rc);
	return rc;
}

/**
 * Process an incoming puback packet for a socket
 * @param pack pointer to the publish packet
 * @param sock the socket on which the packet was received
 * @return completion code
 */
int MQTTProtocol_handlePubacks(void* pack, int sock)
{
	Puback* puback = (Puback*)pack;
	Clients* client = NULL;
	int rc = TCPSOCKET_COMPLETE;

	FUNC_ENTRY;
	client = (Clients*)(ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
	Log(LOG_PROTOCOL, 14, NULL, sock, client->clientID, puback->msgId);

	/* look for the message by message id in the records of outbound messages for this client */
	if (ListFindItem(client->outboundMsgs, &(puback->msgId), messageIDCompare) == NULL)
		Log(TRACE_MIN, 3, NULL, "PUBACK", client->clientID, puback->msgId);
	else
	{
		Messages* m = (Messages*)(client->outboundMsgs->current->content);
		if (m->qos != 1)
			Log(TRACE_MIN, 4, NULL, "PUBACK", client->clientID, puback->msgId, m->qos);
		else
		{
			Log(TRACE_MIN, 6, NULL, "PUBACK", client->clientID, puback->msgId);
			#if !defined(NO_PERSISTENCE)
				rc = MQTTPersistence_remove(client, PERSISTENCE_PUBLISH_SENT, m->qos, puback->msgId);
			#endif
			MQTTProtocol_removePublication(m->publish);
			ListRemove(client->outboundMsgs, m);
		}
	}
	free(pack);
	FUNC_EXIT_RC(rc);
	return rc;
}


/**
 * Process an incoming pubrec packet for a socket
 * @param pack pointer to the publish packet
 * @param sock the socket on which the packet was received
 * @return completion code
 */
int MQTTProtocol_handlePubrecs(void* pack, int sock)
{
	Pubrec* pubrec = (Pubrec*)pack;
	Clients* client = NULL;
	int rc = TCPSOCKET_COMPLETE;

	FUNC_ENTRY;
	client = (Clients*)(ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
	Log(LOG_PROTOCOL, 15, NULL, sock, client->clientID, pubrec->msgId);

	/* look for the message by message id in the records of outbound messages for this client */
	client->outboundMsgs->current = NULL;
	if (ListFindItem(client->outboundMsgs, &(pubrec->msgId), messageIDCompare) == NULL)
	{
		if (pubrec->header.bits.dup == 0)
			Log(TRACE_MIN, 3, NULL, "PUBREC", client->clientID, pubrec->msgId);
	}
	else
	{
		Messages* m = (Messages*)(client->outboundMsgs->current->content);
		if (m->qos != 2)
		{
			if (pubrec->header.bits.dup == 0)
				Log(TRACE_MIN, 4, NULL, "PUBREC", client->clientID, pubrec->msgId, m->qos);
		}
		else if (m->nextMessageType != PUBREC)
		{
			if (pubrec->header.bits.dup == 0)
				Log(TRACE_MIN, 5, NULL, "PUBREC", client->clientID, pubrec->msgId);
		}
		else
		{
			rc = MQTTPacket_send_pubrel(pubrec->msgId, 0, sock, client->clientID);
			m->nextMessageType = PUBCOMP;
			time(&(m->lastTouch));
		}
	}
	free(pack);
	FUNC_EXIT_RC(rc);
	return rc;
}


/**
 * Process an incoming pubrel packet for a socket
 * @param pack pointer to the publish packet
 * @param sock the socket on which the packet was received
 * @return completion code
 */
int MQTTProtocol_handlePubrels(void* pack, int sock)
{
	Pubrel* pubrel = (Pubrel*)pack;
	Clients* client = NULL;
	int rc = TCPSOCKET_COMPLETE;

	FUNC_ENTRY;
	client = (Clients*)(ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
	Log(LOG_PROTOCOL, 17, NULL, sock, client->clientID, pubrel->msgId);

	/* look for the message by message id in the records of inbound messages for this client */
	if (ListFindItem(client->inboundMsgs, &(pubrel->msgId), messageIDCompare) == NULL)
	{
		if (pubrel->header.bits.dup == 0)
			Log(TRACE_MIN, 3, NULL, "PUBREL", client->clientID, pubrel->msgId);
		else
			/* Apparently this is "normal" behaviour, so we don't need to issue a warning */
			rc = MQTTPacket_send_pubcomp(pubrel->msgId, sock, client->clientID);
	}
	else
	{
		Messages* m = (Messages*)(client->inboundMsgs->current->content);
		if (m->qos != 2)
			Log(TRACE_MIN, 4, NULL, "PUBREL", client->clientID, pubrel->msgId, m->qos);
		else if (m->nextMessageType != PUBREL)
			Log(TRACE_MIN, 5, NULL, "PUBREL", client->clientID, pubrel->msgId);
		else
		{
			Publish publish;

			/* send pubcomp before processing the publications because a lot of return publications could fill up the socket buffer */
			rc = MQTTPacket_send_pubcomp(pubrel->msgId, sock, client->clientID);
			publish.header.bits.qos = m->qos;
			publish.header.bits.retain = m->retain;
			publish.msgId = m->msgid;
			publish.topic = m->publish->topic;
			publish.topiclen = m->publish->topiclen;
			publish.payload = m->publish->payload;
			publish.payloadlen = m->publish->payloadlen;
			Protocol_processPublication(&publish, client);
			#if !defined(NO_PERSISTENCE)
				rc = MQTTPersistence_remove(client, PERSISTENCE_PUBLISH_RECEIVED, m->qos, pubrel->msgId);
			#endif
			ListRemove(&(state.publications), m->publish);
			ListRemove(client->inboundMsgs, m);
			++(state.msgs_received);
		}
	}
	free(pack);
	FUNC_EXIT_RC(rc);
	return rc;
}


/**
 * Process an incoming pubcomp packet for a socket
 * @param pack pointer to the publish packet
 * @param sock the socket on which the packet was received
 * @return completion code
 */
int MQTTProtocol_handlePubcomps(void* pack, int sock)
{
	Pubcomp* pubcomp = (Pubcomp*)pack;
	Clients* client = NULL;
	int rc = TCPSOCKET_COMPLETE;

	FUNC_ENTRY;
	client = (Clients*)(ListFindItem(bstate->clients, &sock, clientSocketCompare)->content);
	Log(LOG_PROTOCOL, 19, NULL, sock, client->clientID, pubcomp->msgId);

	/* look for the message by message id in the records of outbound messages for this client */
	if (ListFindItem(client->outboundMsgs, &(pubcomp->msgId), messageIDCompare) == NULL)
	{
		if (pubcomp->header.bits.dup == 0)
			Log(TRACE_MIN, 3, NULL, "PUBCOMP", client->clientID, pubcomp->msgId);
	}
	else
	{
		Messages* m = (Messages*)(client->outboundMsgs->current->content);
		if (m->qos != 2)
			Log(TRACE_MIN, 4, NULL, "PUBCOMP", client->clientID, pubcomp->msgId, m->qos);
		else
		{
			if (m->nextMessageType != PUBCOMP)
				Log(TRACE_MIN, 5, NULL, "PUBCOMP", client->clientID, pubcomp->msgId);
			else
			{
				Log(TRACE_MIN, 6, NULL, "PUBCOMP", client->clientID, pubcomp->msgId);
				#if !defined(NO_PERSISTENCE)
					rc = MQTTPersistence_remove(client, PERSISTENCE_PUBLISH_SENT, m->qos, pubcomp->msgId);
				#endif
				MQTTProtocol_removePublication(m->publish);
				ListRemove(client->outboundMsgs, m);
				(++state.msgs_sent);
			}
		}
	}
	free(pack);
	FUNC_EXIT_RC(rc);
	return rc;
}


/**
 * MQTT protocol keepAlive processing.  Sends PINGREQ packets as required.
 * @param now current time
 */
void MQTTProtocol_keepalive(time_t now)
{
	ListElement* current = NULL;

	FUNC_ENTRY;
	ListNextElement(bstate->clients, &current);
	while (current)
	{
		Clients* client =	(Clients*)(current->content);
		ListNextElement(bstate->clients, &current);
		if (client->connected && client->keepAliveInterval > 0
				&& (difftime(now, client->lastContact) >= client->keepAliveInterval))
		{
			MQTTPacket_send_pingreq(client->socket, client->clientID);
			client->lastContact = now;
			client->ping_outstanding = 1;
		}
	}
	FUNC_EXIT;
}


/**
 * MQTT retry processing per client
 * @param now current time
 * @param client - the client to which to apply the retry processing
 */
void MQTTProtocol_retries(time_t now, Clients* client)
{
	ListElement* outcurrent = NULL;

	FUNC_ENTRY;

	if (client->retryInterval <= 0) /* 0 or -ive retryInterval turns off retry */
		goto exit;

	while (client && ListNextElement(client->outboundMsgs, &outcurrent))
	{
		Messages* m = (Messages*)(outcurrent->content);
		if (difftime(now, m->lastTouch) > max(client->retryInterval, 10))
		{
			if (m->qos == 1 || (m->qos == 2 && m->nextMessageType == PUBREC))
			{
				Publish publish;
				int rc;

				Log(TRACE_MIN, 7, NULL, "PUBLISH", client->clientID, client->socket, m->msgid);
				publish.msgId = m->msgid;
				publish.topic = m->publish->topic;
				publish.payload = m->publish->payload;
				publish.payloadlen = m->publish->payloadlen;
				rc = MQTTPacket_send_publish(&publish, 1, m->qos, m->retain, client->socket, client->clientID);
				if (rc == SOCKET_ERROR)
				{
					client->good = 0;
					Log(TRACE_MIN, 8, NULL, client->clientID, client->socket,
												Socket_getpeer(client->socket));
					MQTTProtocol_closeSession(client, 1);
					client = NULL;
				}
				else
				{
					if (m->qos == 0 && rc == TCPSOCKET_INTERRUPTED)
						MQTTProtocol_storeQoS0(client, &publish);
					time(&(m->lastTouch));
				}
			}
			else if (m->qos && m->nextMessageType == PUBCOMP)
			{
				Log(TRACE_MIN, 7, NULL, "PUBREL", client->clientID, client->socket, m->msgid);
				if (MQTTPacket_send_pubrel(m->msgid, 1, client->socket, client->clientID) != TCPSOCKET_COMPLETE)
				{
					client->good = 0;
					Log(TRACE_MIN, 8, NULL, client->clientID, client->socket,
							Socket_getpeer(client->socket));
					MQTTProtocol_closeSession(client, 1);
					client = NULL;
				}
				else
					time(&(m->lastTouch));
			}
			/* break; why not do all retries at once? */
		}
	}
exit:
	FUNC_EXIT;
}


/**
 * MQTT retry protocol and socket pending writes processing.
 * @param now current time
 * @param doRetry boolean - retries as well as pending writes?
 */
void MQTTProtocol_retry(time_t now, int doRetry)
{
	ListElement* current = NULL;

	FUNC_ENTRY;
	ListNextElement(bstate->clients, &current);
	/* look through the outbound message list of each client, checking to see if a retry is necessary */
	while (current)
	{
		Clients* client = (Clients*)(current->content);
		ListNextElement(bstate->clients, &current);
		if (client->connected == 0)
			continue;
		if (client->good == 0)
		{
			MQTTProtocol_closeSession(client, 1);
			continue;
		}
		if (Socket_noPendingWrites(client->socket) == 0)
			continue;
		if (doRetry)
			MQTTProtocol_retries(now, client);
	}
	FUNC_EXIT;
}


/**
 * Free a client structure
 * @param client the client data to free
 */
void MQTTProtocol_freeClient(Clients* client)
{
	FUNC_ENTRY;
	/* free up pending message lists here, and any other allocated data */
	MQTTProtocol_freeMessageList(client->outboundMsgs);
	MQTTProtocol_freeMessageList(client->inboundMsgs);
	ListFree(client->messageQueue);
	free(client->clientID);
	/*if (client->will != NULL)
	{
		willMessages* w = client->will;
		free(w->msg);
		free(w->topic);
		free(client->will);
	}*/
	/* don't free the client structure itself... this is done elsewhere */
	FUNC_EXIT;
}


/**
 * Empty a message list, leaving it able to accept new messages
 * @param msgList the message list to empty
 */
void MQTTProtocol_emptyMessageList(List* msgList)
{
	ListElement* current = NULL;

	FUNC_ENTRY;
	while (ListNextElement(msgList, &current))
	{
		Messages* m = (Messages*)(current->content);
		MQTTProtocol_removePublication(m->publish);
	}
	ListEmpty(msgList);
	FUNC_EXIT;
}


/**
 * Empty and free up all storage used by a message list
 * @param msgList the message list to empty and free
 */
void MQTTProtocol_freeMessageList(List* msgList)
{
	FUNC_ENTRY;
	MQTTProtocol_emptyMessageList(msgList);
	ListFree(msgList);
	FUNC_EXIT;
}


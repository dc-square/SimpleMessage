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
 * \brief functions to deal with reading and writing of MQTT packets from and to sockets
 *
 * Some other related functions are in the MQTTPacket module
 */


#include "MQTTPacketOut.h"
#include "Log.h"
#include "StackTrace.h"

#include <string.h>
#include <stdlib.h>

#include "Heap.h"


/**
 * Send an MQTT CONNECT packet down a socket.
 * @param client a structure from which to get all the required values
 * @return the completion code (e.g. TCPSOCKET_COMPLETE)
 */
int MQTTPacket_send_connect(Clients* client)
{
	char *buf, *ptr;
	Connect packet;
	int rc, len;

	FUNC_ENTRY;
	packet.header.byte = 0;
	packet.header.bits.type = CONNECT;
	packet.header.bits.qos = 1;

	len = 12 + strlen(client->clientID)+2;
	if (client->will)
		len += strlen(client->will->topic)+2 + strlen(client->will->msg)+2;
	if (client->username)
		len += strlen(client->username)+2;
	if (client->password)
		len += strlen(client->password)+2;

	ptr = buf = malloc(len);
	writeUTF(&ptr, "MQIsdp");
	writeChar(&ptr, (char)3);

	packet.flags.all = 0;
	packet.flags.bits.cleanstart = client->cleansession;
	packet.flags.bits.will = (client->will) ? 1 : 0;
	if (packet.flags.bits.will)
	{
		packet.flags.bits.willQoS = client->will->qos;
		packet.flags.bits.willRetain = client->will->retained;
	}

	if (client->username)
		packet.flags.bits.username = 1;
	if (client->password)
		packet.flags.bits.password = 1;

	writeChar(&ptr, packet.flags.all);
	writeInt(&ptr, client->keepAliveInterval);
	writeUTF(&ptr, client->clientID);
	if (client->will)
	{
		writeUTF(&ptr, client->will->topic);
		writeUTF(&ptr, client->will->msg);
	}
	if (client->username)
		writeUTF(&ptr, client->username);
	if (client->password)
		writeUTF(&ptr, client->password);

	rc = MQTTPacket_send(client->socket, packet.header, buf, len);
	Log(LOG_PROTOCOL, 0, NULL, client->socket, client->clientID, client->cleansession, rc);
	free(buf);
	FUNC_EXIT_RC(rc);
	return rc;
}


/**
 * Function used in the new packets table to create connack packets.
 * @param aHeader the MQTT header byte
 * @param data the rest of the packet
 * @param datalen the length of the rest of the packet
 * @return pointer to the packet structure
 */
void* MQTTPacket_connack(unsigned char aHeader, char* data, int datalen)
{
	Connack* pack = malloc(sizeof(Connack));
	char* curdata = data;

	FUNC_ENTRY;
	pack->header.byte = aHeader;
	readChar(&curdata);	/* reserved byte */
	pack->rc = readChar(&curdata);
	FUNC_EXIT;
	return pack;
}


/**
 * Send an MQTT PINGREQ packet down a socket.
 * @param socket the open socket to send the data to
 * @param clientID the string client identifier, only used for tracing
 * @return the completion code (e.g. TCPSOCKET_COMPLETE)
 */
int MQTTPacket_send_pingreq(int socket, char* clientID)
{
	Header header;
	int rc = 0;

	FUNC_ENTRY;
	header.byte = 0;
	header.bits.type = PINGREQ;
	rc = MQTTPacket_send(socket, header, NULL, 0);
	Log(LOG_PROTOCOL, 20, NULL, socket, clientID, rc);
	FUNC_EXIT_RC(rc);
	return rc;
}


/**
 * Send an MQTT subscribe packet down a socket.
 * @param topics list of topics
 * @param qoss list of corresponding QoSs
 * @param msgid the MQTT message id to use
 * @param dup boolean - whether to set the MQTT DUP flag
 * @param socket the open socket to send the data to
 * @param clientID the string client identifier, only used for tracing
 * @return the completion code (e.g. TCPSOCKET_COMPLETE)
 */
int MQTTPacket_send_subscribe(List* topics, List* qoss, int msgid, int dup, int socket, char* clientID)
{
	Header header;
	char *data, *ptr;
	int rc = -1;
	ListElement *elem = NULL, *qosElem = NULL;
	int datalen;

	FUNC_ENTRY;
	header.bits.type = SUBSCRIBE;
	header.bits.dup = dup;
	header.bits.qos = 1;
	header.bits.retain = 0;

	datalen = 2 + topics->count * 3; // utf length + char qos == 3
	while (ListNextElement(topics, &elem))
		datalen += strlen((char*)(elem->content));
	ptr = data = malloc(datalen);

	writeInt(&ptr, msgid);
	elem = NULL;
	while (ListNextElement(topics, &elem))
	{
		ListNextElement(qoss, &qosElem);
		writeUTF(&ptr, (char*)(elem->content));
		writeChar(&ptr, *(int*)(qosElem->content));
	}
	rc = MQTTPacket_send(socket, header, data, datalen);
	Log(LOG_PROTOCOL, 22, NULL, socket, clientID, msgid, rc);
	free(data);
	FUNC_EXIT_RC(rc);
	return rc;
}


/**
 * Function used in the new packets table to create suback packets.
 * @param aHeader the MQTT header byte
 * @param data the rest of the packet
 * @param datalen the length of the rest of the packet
 * @return pointer to the packet structure
 */
void* MQTTPacket_suback(unsigned char aHeader, char* data, int datalen)
{
	Suback* pack = malloc(sizeof(Suback));
	char* curdata = data;

	FUNC_ENTRY;
	pack->header.byte = aHeader;
	pack->msgId = readInt(&curdata);
	pack->qoss = ListInitialize();
	while (curdata - data < datalen)
	{
		int* newint;
		newint = malloc(sizeof(int));
		*newint = (int)readChar(&curdata);
		ListAppend(pack->qoss, newint, sizeof(int));
	}
	FUNC_EXIT;
	return pack;
}


/**
 * Send an MQTT unsubscribe packet down a socket.
 * @param topics list of topics
 * @param msgid the MQTT message id to use
 * @param dup boolean - whether to set the MQTT DUP flag
 * @param socket the open socket to send the data to
 * @param clientID the string client identifier, only used for tracing
 * @return the completion code (e.g. TCPSOCKET_COMPLETE)
 */
int MQTTPacket_send_unsubscribe(List* topics, int msgid, int dup, int socket, char* clientID)
{
	Header header;
	char *data, *ptr;
	int rc = -1;
	ListElement *elem = NULL;
	int datalen;

	FUNC_ENTRY;
	header.bits.type = UNSUBSCRIBE;
	header.bits.dup = dup;
	header.bits.qos = 1;
	header.bits.retain = 0;

	datalen = 2 + topics->count * 2; // utf length == 2
	while (ListNextElement(topics, &elem))
		datalen += strlen((char*)(elem->content));
	ptr = data = malloc(datalen);

	writeInt(&ptr, msgid);
	elem = NULL;
	while (ListNextElement(topics, &elem))
		writeUTF(&ptr, (char*)(elem->content));
	rc = MQTTPacket_send(socket, header, data, datalen);
	Log(LOG_PROTOCOL, 25, NULL, socket, clientID, msgid, rc);
	free(data);
	FUNC_EXIT_RC(rc);
	return rc;
}

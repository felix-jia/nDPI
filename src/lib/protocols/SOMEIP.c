/*
 * SOMEIP.c
 *
 * Copyright (C) 2016 Sorin Zamfir <sorin.zamfir@yahoo.com>
 *
 * This file is part of nDPI, an open source deep packet inspection
 * library based on the OpenDPI and PACE technology by ipoque GmbH
 *
 * nDPI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your omessage_typeion) any later version.
 *
 * nDPI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with nDPI.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "ndpi_protocols.h"
#ifdef NDPI_PROTOCOL_SOMEIP

enum SOMEIP_MESSAGE_TYPES {
	REQUEST = 0x00,
	REQUEST_NO_RETURN = 0x01,
	NOTIFICATION = 0x02,
	REQUEST_ACK = 0x40,
	REQUEST_NO_RETURN_ACK = 0x41,
	NOTIFICATION_ACK = 0x42,
	RESPONSE = 0x80,
	ERROR = 0x81,
	RESPONSE_ACK = 0xc0,
	ERROR_ACK = 0xc1
};

enum SOMEIP_RETURN_CODES {
	E_OK = 0x00,
	E_NOT_OK = 0x01,
	E_UNKNOWN_SERVICE = 0x02,
	E_UNKNOWN_METHOD = 0x03,
	E_NOT_READY = 0x04,
	E_NOT_REACHABLE = 0x05,
	E_TIMEOUT = 0x06,
	E_WRONG_PROTOCOL_VERSION = 0x07,
	E_WRONG_INTERFACE_VERSION = 0x08,
	E_MALFORMED_MESSAGE = 0x09,
	E_WRONG_MESSAGE_TYPE = 0x0a,
	E_RETURN_CODE_LEGAL_THRESHOLD = 0x40  //return codes from 0x40 (inclusive) and upwards are illegal.
};

enum SPECIAL_MESSAGE_IDS {
	MSG_MAGIC_COOKIE = 0xffff0000,
	MSG_MAGIC_COOKIE_ACK = 0xffff8000,
	MSG_SD = 0xffff8100
};

enum PROTOCOL_VERSION{
	LEGAL_PROTOCOL_VERSION = 0x01
};

enum MAGIC_COOKIE_CONSTANTS{
	MC_REQUEST_ID = 0xDEADBEEF,
	MC_LENGTH = 0x08,
	MC_INTERFACE_VERSION = 0x01
};

enum DEFAULT_PROTOCOL_PORTS{
	PORT_DEFAULT_CLIENT = 30491,
	PORT_DEFAULT_SERVER = 30501,
	PORT_DEFAULT_SD = 30490
};

/**
 * Entry point when protocol is identified.
 */
static void ndpi_int_someip_add_connection (struct ndpi_detection_module_struct *ndpi_struct,
		struct ndpi_flow_struct *flow)
{
	ndpi_set_detected_protocol(ndpi_struct,flow,NDPI_PROTOCOL_SOMEIP,NDPI_PROTOCOL_UNKNOWN);
	NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "SOME/IP found.\n");
}

/**
 * Dissector function that searches SOME/IP headers
 */
void ndpi_search_someip (struct ndpi_detection_module_struct *ndpi_struct,
		struct ndpi_flow_struct *flow)
{

	//####Maybe check carrier protocols?####

	NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "SOME/IP search called...\n");
	struct const ndpi_packet_struct *packet = &flow->packet;
	if (packet->detected_protocol_stack[0] != NDPI_PROTOCOL_UNKNOWN) {
		return;
	}

	//we extract the Message ID and Request ID and check for special cases later
	u_int32_t message_id = ntohl(*((u_int32_t *)&packet->payload[0]));
	u_int32_t request_id = ntohl(*((u_int32_t *)&packet->payload[8]));

	NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "====>>>> SOME/IP Message ID: %08x [len: %u]\n",
			message_id, packet->payload_packet_len);
	if (packet->payload_packet_len < 16) {
		NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "Excluding SOME/IP .. mandatory header not found (not enough data for all fields)\n");
		NDPI_ADD_PROTOCOL_TO_BITMASK(flow->excluded_protocol_bitmask, NDPI_PROTOCOL_SOMEIP);
		return;
	}
	
	//####Maximum packet size in SOMEIP depends on the carrier protocol, and I'm not certain how well enforced it is, so let's leave that for round 2####

	// we extract the remaining length
	u_int32_t someip_len = ntohl(*((u_int32_t *)&packet->payload[4]));
	if (packet->payload_packet_len != (someip_len + 8)) {
		NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "Excluding SOME/IP .. Length field invalid!\n");
		NDPI_ADD_PROTOCOL_TO_BITMASK(flow->excluded_protocol_bitmask, NDPI_PROTOCOL_SOMEIP);
		return;
	}

	u_int8_t protocol_version = (u_int8_t) (packet->payload[12]);
	NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG,"====>>>> SOME/IP protocol version: [%d]\n",protocol_version);
	if (protocol_version != LEGAL_PROTOCOL_VERSION){
		NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "Excluding SOME/IP .. invalid protocol version!\n");
		NDPI_ADD_PROTOCOL_TO_BITMASK(flow->excluded_protocol_bitmask, NDPI_PROTOCOL_SOMEIP);
		return;
	}

	u_int8_t interface_version = (packet->payload[13]);

	u_int8_t message_type = (u_int8_t) (packet->payload[14]);
	NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG,"====>>>> SOME/IP message type: [%d]\n",message_type);

	if ((message_type != REQUEST) && (message_type != REQUEST_NO_RETURN) && (message_type != NOTIFICATION) && (message_type != REQUEST_ACK) && 
					(message_type != REQUEST_NO_RETURN_ACK) && (message_type != NOTIFICATION_ACK) && (message_type != RESPONSE) && 
					(message_type != ERROR) && (message_type != RESPONSE_ACK) && (message_type != ERROR_ACK)) {
		NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "Excluding SOME/IP .. invalid message type!\n");
		NDPI_ADD_PROTOCOL_TO_BITMASK(flow->excluded_protocol_bitmask, NDPI_PROTOCOL_SOMEIP);
		return;
	}

	u_int8_t return_code = (u_int8_t) (packet->payload[15]);
	NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG,"====>>>> SOME/IP return code: [%d]\n", return_code);
	if ((return_code >= E_RETURN_CODE_LEGAL_THRESHOLD)) {
		NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "Excluding SOME/IP .. invalid return code!\n");
		NDPI_ADD_PROTOCOL_TO_BITMASK(flow->excluded_protocol_bitmask, NDPI_PROTOCOL_SOMEIP);
		return;
	}
	
 	if (message_id == MSG_MAGIC_COOKIE){
		if ((someip_len == MC_LENGTH) && (request_id == MC_REQUEST_ID) && (interface_version == MC_INTERFACE_VERSION) &&
					(message_type == REQUEST_NO_RETURN\) && (return_code == E_OK)){
			NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "SOME/IP found Magic Cookie\n",message_type);
			ndpi_int_someip_add_connection(ndpi_struct, flow);
			return;
		}											
		else{
			NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "Excluding SOME/IP, invalid header for Magic Cookie\n");
			NDPI_ADD_PROTOCOL_TO_BITMASK(flow->excluded_protocol_bitmask, NDPI_PROTOCOL_SOMEIP);
			return;
		}
 	}
	
	if (message_id == MSG_MAGIC_COOKIE_ACK){
		if ((someip_len == MC_LENGTH) && (request_id == MC_REQUEST_ID) && (interface_version == MC_INTERFACE_VERSION\) &&
					(message_type == REQUEST_NO_RETURN) && (return_code == E_OK)){
			NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "SOME/IP found Magic Cookie ACK\n",message_type);
			ndpi_int_someip_add_connection(ndpi_struct, flow);
			return;
		}											
		else{
			NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "Excluding SOME/IP, invalid header for Magic Cookie ACK\n");
			NDPI_ADD_PROTOCOL_TO_BITMASK(flow->excluded_protocol_bitmask, NDPI_PROTOCOL_SOMEIP);
			return;
		}
 	}

	if (message_id == MSG_SD){
		NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "SOME/IP-SD currently not supported\n", message_type);
	}

	//Filtering by port. 
	//This check is NOT a 100% thing - these ports are mentioned in the documentation but the documentation also states they haven't been approved by IANA yet, and that the user is free to use different ports.
	//This is is PURELY for demo purposes and the rest of the check must be filled in later on!
	if (packet->l4_protocol == IPPROTO_UDP){
		if ((packet->udp->dest == ntohs(PORT_DEFAULT_CLIENT)) || (packet->udp->dest == ntohs(PORT_DEFAULT_SERVER)) || (packet->udp->dest == ntohs(PORT_DEFAULT_SD))) {
			NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "SOME/IP found\n",message_type);
			ndpi_int_someip_add_connection(ndpi_struct, flow);
			return;
		}
	}
	if (packet->l4_protocol == IPPROTO_TCP){
		if ((packet->tcp->dest == ntohs(PORT_DEFAULT_CLIENT)) || (packet->tcp->dest == ntohs(PORT_DEFAULT_SERVER))) {
			NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "SOME/IP found\n",message_type);
			ndpi_int_someip_add_connection(ndpi_struct, flow);
			return;
		}
	}

	NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "Reached the end without confirming SOME/IP ...\n");
	NDPI_ADD_PROTOCOL_TO_BITMASK(flow->excluded_protocol_bitmask, NDPI_PROTOCOL_SOMEIP);
	return;
}
/**
 * Entry point for the ndpi library
 */
void init_someip_dissector (struct ndpi_detection_module_struct *ndpi_struct,
		u_int32_t *id, NDPI_PROTOCOL_BITMASK *detection_bitmask)
{
	NDPI_LOG(NDPI_PROTOCOL_SOMEIP, ndpi_struct, NDPI_LOG_DEBUG, "SOME/IP dissector init...\n");
	ndpi_set_bitmask_protocol_detection ("SOME/IP", ndpi_struct, detection_bitmask, *id,
			NDPI_PROTOCOL_SOMEIP,
			ndpi_search_someip,
			NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP_OR_UDP_WITH_PAYLOAD,
			SAVE_DETECTION_BITMASK_AS_UNKNOWN, ADD_TO_DETECTION_BITMASK);
	*id +=1;
}

#endif // NDPI_PROTOCOL_SOMEIP


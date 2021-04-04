#ifndef __NETWORK_SPNEGO_H__
#define __NETWORK_SPNEGO_H__

#include <glib.h>

/**
 * SPNEGO_OID:
 *
 * OID of SPNEGO
 */
#define SPNEGO_OID         "1.3.6.1.5.5.2"
/**
 * SPNEGO_OID_NTLM:
 *
 * OID of SPNEGO NTLM
 */
#define SPNEGO_OID_NTLM    "1.3.6.1.4.1.311.2.2.10"
/**
 * SPNEGO_OID_MS_KRB5:
 *
 * OID of SPNEGO MS_KRB5
 */
#define SPNEGO_OID_MS_KRB5 "1.2.840.48018.1.2.2"
/**
 * SPNEGO_OID_KRB5:
 *
 * OID of SPNEGO Kerberos5
 */
#define SPNEGO_OID_KRB5    "1.2.840.113554.1.2.2"
/**
 * SPNEGO_OID_NEGOEX:
 *
 * OID of SPNEGO NegoEx
 */
#define SPNEGO_OID_NEGOEX  "1.3.6.1.4.1.311.2.2.30"

/**
 * network_spnego_response_state:
 * @SPNEGO_RESPONSE_STATE_ACCEPT_COMPLETED: completed
 * @SPNEGO_RESPONSE_STATE_ACCEPT_INCOMPLETE: incomplete
 * @SPNEGO_RESPONSE_STATE_REJECTED: rejected
 * @SPNEGO_RESPONSE_STATE_MICSOMETHING: mic something
 *
 * state of the response
 *
 * as it is incomplete more packets need to be exchanged
 */
typedef enum {
	SPNEGO_RESPONSE_STATE_ACCEPT_COMPLETED,
	SPNEGO_RESPONSE_STATE_ACCEPT_INCOMPLETE,
	SPNEGO_RESPONSE_STATE_REJECTED,
	SPNEGO_RESPONSE_STATE_MICSOMETHING
} network_spnego_response_state;

/**
 * network_spnego_response_token:
 * @negState: negotiation state
 * @supportedMech: supported mech
 * @responseToken: response token
 * @mechListMIC: mech list mIC
 *
 * the response token
 */
typedef struct {
	network_spnego_response_state negState;

	GString *supportedMech;

	GString *responseToken;
	GString *mechListMIC;
} network_spnego_response_token;

/**
 * network_spnego_init_token:
 * @mechTypes: (element-type char): mech types
 * @mechToken: mech token
 *
 * init token
 */
typedef struct {
	GPtrArray *mechTypes; /* array of strings */

	GString *mechToken;
} network_spnego_init_token;

gboolean
network_gssapi_proto_get_message_header(network_packet *packet, GString *oid, GError **gerr);

network_spnego_response_token *
network_spnego_response_token_new(void);

void 
network_spnego_response_token_free(network_spnego_response_token *token);

gboolean
network_spnego_proto_get_response_token(network_packet *packet, network_spnego_response_token *token, GError **gerr);

network_spnego_init_token *
network_spnego_init_token_new(void);

void
network_spnego_init_token_free(network_spnego_init_token *token);

gboolean
network_spnego_proto_get_init_token(network_packet *packet, network_spnego_init_token *token, GError **gerr);

#endif

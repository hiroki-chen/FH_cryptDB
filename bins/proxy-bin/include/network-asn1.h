#ifndef __NETWORK_ASN1_H__
#define __NETWORK_ASN1_H__

#include <glib.h>

#include "network-exports.h"

/**
 * ASN1IdentifierKlass:
 * @ASN1_IDENTIFIER_KLASS_UNIVERSAL: universal
 * @ASN1_IDENTIFIER_KLASS_APPLICATION: application
 * @ASN1_IDENTIFIER_KLASS_CONTEXT_SPECIFIC: context specific
 * @ASN1_IDENTIFIER_KLASS_PRIVATE: private
 *
 * ASN1 ID class
 */
typedef enum {
	ASN1_IDENTIFIER_KLASS_UNIVERSAL,
	ASN1_IDENTIFIER_KLASS_APPLICATION,
	ASN1_IDENTIFIER_KLASS_CONTEXT_SPECIFIC,
	ASN1_IDENTIFIER_KLASS_PRIVATE
} ASN1IdentifierKlass;

/**
 * ASN1IdentifierType:
 * @ASN1_IDENTIFIER_TYPE_PRIMITIVE: primitive
 * @ASN1_IDENTIFIER_TYPE_CONSTRUCTED: constructed
 *
 * ASN1 ID type
 */
typedef enum {
	ASN1_IDENTIFIER_TYPE_PRIMITIVE,
	ASN1_IDENTIFIER_TYPE_CONSTRUCTED
} ASN1IdentifierType;

/**
 * ASN1Identifier:
 * @klass: a #ASN1IdentifierKlass
 * @type: a #ASN1IdentifierType
 * @value: value
 *
 * ASN1 identifier
 */
typedef struct {
	ASN1IdentifierKlass klass;
	ASN1IdentifierType type;
	guint64 value; /* we don't support larger values */
} ASN1Identifier;

/**
 * ASN1Length:
 *
 * length of ASN1 values
 *
 * while ASN1 support infinite length we only support 2^64-1 bytes
 */
typedef guint64 ASN1Length;

/**
 * ASN1IdentifierUniversalType:
 * @ASN1_IDENTIFIER_UNIVERSAL_OCTET_STREAM: octet-stream
 * @ASN1_IDENTIFIER_UNIVERSAL_OID: OID
 * @ASN1_IDENTIFIER_UNIVERSAL_ENUM: enum
 * @ASN1_IDENTIFIER_UNIVERSAL_SEQUENCE: sequence
 *
 * (subset of) ASN1 ID Universal type
 */
typedef enum {
	ASN1_IDENTIFIER_UNIVERSAL_OCTET_STREAM = 0x04,
	ASN1_IDENTIFIER_UNIVERSAL_OID = 0x06,
	ASN1_IDENTIFIER_UNIVERSAL_ENUM = 0x0a,
	ASN1_IDENTIFIER_UNIVERSAL_SEQUENCE = 0x10
} ASN1IdentifierUniversalType;

/**
 * NETWORK_ASN1_ERROR:
 *
 * error domain for all errors of #ASN1Error
 */
#define NETWORK_ASN1_ERROR network_asn1_error()
GQuark
network_asn1_error(void);

/**
 * ASN1Error:
 * @NETWORK_ASN1_ERROR_UNSUPPORTED: unsupported
 * @NETWORK_ASN1_ERROR_INVALID: invalid
 * @NETWORK_ASN1_ERROR_EOF: EOF
 *
 * errors of the ASN1Error domain
 */
typedef enum {
	NETWORK_ASN1_ERROR_UNSUPPORTED,
	NETWORK_ASN1_ERROR_INVALID,
	NETWORK_ASN1_ERROR_EOF
} ASN1Error;

gboolean
network_asn1_is_valid(network_packet *packet, GError **gerr);

gboolean
network_asn1_proto_get_oid(network_packet *packet, ASN1Length len, GString *oid, GError **gerr);

gboolean
network_asn1_proto_get_header(network_packet *packet, ASN1Identifier *_id, ASN1Length *_len, GError **gerr);

gboolean
network_asn1_proto_get_length(network_packet *packet, ASN1Length *_len, GError **gerr);

gboolean
network_asn1_proto_get_id(network_packet *packet, ASN1Identifier *id, GError **gerr);

#endif

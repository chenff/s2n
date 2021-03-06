/*
 * Copyright 2014 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include "tls/s2n_server_key_exchange.h"
#include "tls/s2n_client_key_exchange.h"
#include "tls/s2n_kex.h"
#include "tls/s2n_cipher_suites.h"
#include "utils/s2n_safety.h"
#include "s2n_tls.h"

static int s2n_get_server_ecc_extension_size(const struct s2n_connection *conn)
{
    if (s2n_server_can_send_ec_point_formats(conn)){
        return 6;
    } else {
        return 0;
    }
}

static int s2n_get_no_extension_size(const struct s2n_connection *conn)
{
    return 0;
}

/* Write the Supported Points Format extension.
 * RFC 4492 section 5.2 states that the absence of this extension in the Server Hello
 * is equivalent to allowing only the uncompressed point format. Let's send the
 * extension in case clients(Openssl 1.0.0) don't honor the implied behavior.
 */
static int s2n_write_server_ecc_extension(const struct s2n_connection *conn, struct s2n_stuffer *out)
{
    if (s2n_server_can_send_ec_point_formats(conn)) {
        GUARD(s2n_stuffer_write_uint16(out, TLS_EXTENSION_EC_POINT_FORMATS));
        /* Total extension length */
        GUARD(s2n_stuffer_write_uint16(out, 2));
        /* Format list length */
        GUARD(s2n_stuffer_write_uint8(out, 1));
        /* Only uncompressed format is supported. Interoperability shouldn't be an issue:
         * RFC 4492 Section 5.1.2: Implementations must support it for all of their curves.
         */
        GUARD(s2n_stuffer_write_uint8(out, TLS_EC_FORMAT_UNCOMPRESSED));
    }
    return 0;
}

static int s2n_write_no_extension(const struct s2n_connection *conn, struct s2n_stuffer *out)
{
    return 0;
}

static int s2n_check_rsa_key(const struct s2n_connection *conn)
{
    return conn->handshake_params.our_chain_and_key != NULL;
}

static int s2n_check_dhe(const struct s2n_connection *conn)
{
    return conn->config->dhparams != NULL;
}

static int s2n_check_ecdhe(const struct s2n_connection *conn)
{
    return conn->secure.server_ecc_params.negotiated_curve != NULL;
}

static int s2n_check_kem(const struct s2n_connection *conn)
{
    return conn->secure.s2n_kem_keys.negotiated_kem != NULL;
}

static int s2n_check_hybrid(const struct s2n_connection *conn)
{
    const struct s2n_kex *kex = conn->secure.cipher_suite->key_exchange_alg;
    const struct s2n_kex *hybrid_kex_1 = *kex->hybrid;
    const struct s2n_kex *hybrid_kex_2 = hybrid_kex_1 + 1;
    return s2n_kex_supported(hybrid_kex_1, conn) && s2n_kex_supported(hybrid_kex_2, conn);
}

static int s2n_write_server_hybrid_extensions(const struct s2n_connection *conn, struct s2n_stuffer *out)
{
    const struct s2n_kex *kex = conn->secure.cipher_suite->key_exchange_alg;
    const struct s2n_kex *hybrid_kex_1 = *kex->hybrid;
    const struct s2n_kex *hybrid_kex_2 = hybrid_kex_1 + 1;
    GUARD(s2n_kex_write_server_extension(hybrid_kex_1, conn, out));
    GUARD(s2n_kex_write_server_extension(hybrid_kex_2, conn, out));
    return 0;
}

static int s2n_get_server_hybrid_extensions_size(const struct s2n_connection *conn)
{
    const struct s2n_kex *kex = conn->secure.cipher_suite->key_exchange_alg;
    const struct s2n_kex *hybrid_kex_1 = *kex->hybrid;
    const struct s2n_kex *hybrid_kex_2 = hybrid_kex_1 + 1;
    return s2n_kex_server_extension_size(hybrid_kex_1, conn) + s2n_kex_server_extension_size(hybrid_kex_2, conn);
}

static const struct s2n_kex s2n_sike = {
        .is_ephemeral = 1,
        .get_server_extension_size = &s2n_get_no_extension_size,
        .write_server_extensions = &s2n_write_no_extension,
        .connection_supported = &s2n_check_kem,
        .server_key_recv_read_data = &s2n_kem_server_key_recv_read_data,
        .server_key_recv_parse_data = &s2n_kem_server_key_recv_parse_data,
        .server_key_send = &s2n_kem_server_key_send,
        .client_key_recv = &s2n_kem_client_key_recv,
        .client_key_send = &s2n_kem_client_key_send,

};

const struct s2n_kex s2n_rsa = {
        .is_ephemeral = 0,
        .get_server_extension_size = &s2n_get_no_extension_size,
        .write_server_extensions = &s2n_write_no_extension,
        .connection_supported = &s2n_check_rsa_key,
        .server_key_recv_read_data = NULL,
        .server_key_recv_parse_data = NULL,
        .server_key_send = NULL,
        .client_key_recv = &s2n_rsa_client_key_recv,
        .client_key_send = &s2n_rsa_client_key_send,
        .prf = &s2n_tls_prf_master_secret,
};

const struct s2n_kex s2n_dhe = {
        .is_ephemeral = 1,
        .get_server_extension_size = &s2n_get_no_extension_size,
        .write_server_extensions = &s2n_write_no_extension,
        .connection_supported = &s2n_check_dhe,
        .server_key_recv_read_data = &s2n_dhe_server_key_recv_read_data,
        .server_key_recv_parse_data = &s2n_dhe_server_key_recv_parse_data,
        .server_key_send = &s2n_dhe_server_key_send,
        .client_key_recv = &s2n_dhe_client_key_recv,
        .client_key_send = &s2n_dhe_client_key_send,
        .prf = &s2n_tls_prf_master_secret,
};

const struct s2n_kex s2n_ecdhe = {
        .is_ephemeral = 1,
        .get_server_extension_size = &s2n_get_server_ecc_extension_size,
        .write_server_extensions = &s2n_write_server_ecc_extension,
        .connection_supported = &s2n_check_ecdhe,
        .server_key_recv_read_data = &s2n_ecdhe_server_key_recv_read_data,
        .server_key_recv_parse_data = &s2n_ecdhe_server_key_recv_parse_data,
        .server_key_send = &s2n_ecdhe_server_key_send,
        .client_key_recv = &s2n_ecdhe_client_key_recv,
        .client_key_send = &s2n_ecdhe_client_key_send,
        .prf = &s2n_tls_prf_master_secret,
};


const struct s2n_kex s2n_hybrid_ecdhe_sike = {
        .is_ephemeral = 1,
        .hybrid = {&s2n_ecdhe, &s2n_sike},
        .get_server_extension_size = &s2n_get_server_hybrid_extensions_size,
        .write_server_extensions = &s2n_write_server_hybrid_extensions,
        .connection_supported = &s2n_check_hybrid,
        .server_key_recv_read_data = &s2n_hybrid_server_key_recv_read_data,
        .server_key_recv_parse_data = &s2n_hybrid_server_key_recv_parse_data,
        .server_key_send = &s2n_hybrid_server_key_send,
        .client_key_recv = &s2n_hybrid_client_key_recv,
        .client_key_send = &s2n_hybrid_client_key_send,
        .prf = &s2n_hybrid_prf_master_secret,
};

int s2n_kex_server_extension_size(const struct s2n_kex *kex, const struct s2n_connection *conn)
{
    notnull_check(kex->get_server_extension_size);
    return kex->get_server_extension_size(conn);
}

int s2n_kex_write_server_extension(const struct s2n_kex *kex, const struct s2n_connection *conn, struct s2n_stuffer *out)
{
    notnull_check(kex->write_server_extensions);
    return kex->write_server_extensions(conn, out);
}

int s2n_kex_supported(const struct s2n_kex *kex, const struct s2n_connection *conn)
{
    /* Don't return -1 from notnull_check because that might allow a improperly configured kex to be marked as "supported" */
    return kex->connection_supported != NULL && kex->connection_supported(conn);
}

int s2n_kex_is_ephemeral(const struct s2n_kex *kex)
{
    return kex->is_ephemeral;
}

int s2n_kex_server_key_recv_parse_data(const struct s2n_kex *kex, struct s2n_connection *conn, struct s2n_kex_raw_server_data *raw_server_data)
{
    notnull_check(kex->server_key_recv_parse_data);
    return kex->server_key_recv_parse_data(conn, raw_server_data);
}

int s2n_kex_server_key_recv_read_data(const struct s2n_kex *kex, struct s2n_connection *conn, struct s2n_blob *data_to_verify, struct s2n_kex_raw_server_data *raw_server_data)
{
    notnull_check(kex->server_key_recv_read_data);
    return kex->server_key_recv_read_data(conn, data_to_verify, raw_server_data);
}

int s2n_kex_server_key_send(const struct s2n_kex *kex, struct s2n_connection *conn, struct s2n_blob *data_to_sign)
{
    notnull_check(kex->server_key_send);
    return kex->server_key_send(conn, data_to_sign);
}

int s2n_kex_client_key_recv(const struct s2n_kex *kex, struct s2n_connection *conn, struct s2n_blob *shared_key)
{
    notnull_check(kex->client_key_recv);
    return kex->client_key_recv(conn, shared_key);
}

int s2n_kex_client_key_send(const struct s2n_kex *kex, struct s2n_connection *conn, struct s2n_blob *shared_key)
{
    notnull_check(kex->client_key_send);
    return kex->client_key_send(conn, shared_key);
}

int s2n_kex_tls_prf(const struct s2n_kex *kex, struct s2n_connection *conn, struct s2n_blob *premaster_secret)
{
    notnull_check(kex->prf);
    return kex->prf(conn, premaster_secret);
}

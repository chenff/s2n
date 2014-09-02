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

#include <openssl/md5.h>
#include <openssl/sha.h>

#include "crypto/s2n_hash.h"

#include "utils/s2n_safety.h"

int s2n_hash_digest_size(s2n_hash_algorithm alg, const char **err)
{
    int sizes[] = { 0, MD5_DIGEST_LENGTH, SHA_DIGEST_LENGTH, SHA256_DIGEST_LENGTH, SHA384_DIGEST_LENGTH, SHA512_DIGEST_LENGTH, MD5_DIGEST_LENGTH + SHA_DIGEST_LENGTH };

    if (alg >= sizeof(sizes) / sizeof(int)) {
        *err = "Invalid hash algorithm";
        return -1;
    }

    return sizes[alg];
}

int s2n_hash_init(struct s2n_hash_state *state, s2n_hash_algorithm alg, const char **err)
{
    int r;
    switch (alg) {
    case S2N_HASH_NONE:
        r = 1;
        break;
    case S2N_HASH_MD5:
        r = MD5_Init(&state->hash_ctx.md5);
        break;
    case S2N_HASH_SHA1:
        r = SHA1_Init(&state->hash_ctx.sha1);
        break;
    case S2N_HASH_SHA256:
        r = SHA256_Init(&state->hash_ctx.sha256);
        break;
    case S2N_HASH_SHA384:
        r = SHA384_Init(&state->hash_ctx.sha384);
        break;
    case S2N_HASH_SHA512:
        r = SHA512_Init(&state->hash_ctx.sha512);
        break;
    case S2N_HASH_MD5_SHA1:
        r = SHA1_Init(&state->hash_ctx.md5_sha1.sha1);
        r &= MD5_Init(&state->hash_ctx.md5_sha1.md5);
        break;

    default:
        *err = "Invalid hash algorithm";
        return -1;
    }

    if (r == 0) {
        *err = "s2n_hash_init failed";
        return -1;
    }

    state->alg = alg;

    return 0;
}

int s2n_hash_update(struct s2n_hash_state *state, const void *data, uint32_t size, const char **err)
{
    int r;
    switch (state->alg) {
    case S2N_HASH_NONE:
        r = 1;
        break;
    case S2N_HASH_MD5:
        r = MD5_Update(&state->hash_ctx.md5, data, size);
        break;
    case S2N_HASH_SHA1:
        r = SHA1_Update(&state->hash_ctx.sha1, data, size);
        break;
    case S2N_HASH_SHA256:
        r = SHA256_Update(&state->hash_ctx.sha256, data, size);
        break;
    case S2N_HASH_SHA384:
        r = SHA384_Update(&state->hash_ctx.sha384, data, size);
        break;
    case S2N_HASH_SHA512:
        r = SHA512_Update(&state->hash_ctx.sha512, data, size);
        break;
    case S2N_HASH_MD5_SHA1:
        r = SHA1_Update(&state->hash_ctx.md5_sha1.sha1, data, size);
        r &= MD5_Update(&state->hash_ctx.md5_sha1.md5, data, size);
        break;
    default:
        *err = "Invalid hash algorithm";
        return -1;
    }

    if (r == 0) {
        *err = "s2n_hash_update failed";
        return -1;
    }

    return 0;
}

int s2n_hash_digest(struct s2n_hash_state *state, void *out, uint32_t size, const char **err)
{
    int r;
    switch (state->alg) {
    case S2N_HASH_NONE:
        r = 1;
        break;
    case S2N_HASH_MD5:
        eq_check(size, MD5_DIGEST_LENGTH);
        r = MD5_Final(out, &state->hash_ctx.md5);
        break;
    case S2N_HASH_SHA1:
        eq_check(size, SHA_DIGEST_LENGTH);
        r = SHA1_Final(out, &state->hash_ctx.sha1);
        break;
    case S2N_HASH_SHA256:
        eq_check(size, SHA256_DIGEST_LENGTH);
        r = SHA256_Final(out, &state->hash_ctx.sha256);
        break;
    case S2N_HASH_SHA384:
        eq_check(size, SHA384_DIGEST_LENGTH);
        r = SHA512_Final(out, &state->hash_ctx.sha384);
        break;
    case S2N_HASH_SHA512:
        eq_check(size, SHA512_DIGEST_LENGTH);
        r = SHA512_Final(out, &state->hash_ctx.sha512);
        break;
    case S2N_HASH_MD5_SHA1:
        eq_check(size, MD5_DIGEST_LENGTH + SHA_DIGEST_LENGTH);
        r = SHA1_Final(((uint8_t *) out) + MD5_DIGEST_LENGTH, &state->hash_ctx.md5_sha1.sha1);
        r &= MD5_Final(out, &state->hash_ctx.md5_sha1.md5);
        break;
    default:
        *err = "Invalid hash algorithm";
        return -1;
    }

    if (r == 0) {
        *err = "s2n_hash_digest failed";
        return -1;
    }

    return 0;
}

int s2n_hash_reset(struct s2n_hash_state *state, const char **err)
{
    return s2n_hash_init(state, state->alg, err);
}

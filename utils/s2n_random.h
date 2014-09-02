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

#pragma once

#include <openssl/rand.h>
#include <stdint.h>

#include "stuffer/s2n_stuffer.h"

extern int s2n_get_random_data(uint8_t *data, uint32_t n, const char **err);
extern int s2n_stuffer_write_random_data(struct s2n_stuffer *tuffer, uint32_t n, const char **err);

extern RAND_METHOD s2n_openssl_rand_method;

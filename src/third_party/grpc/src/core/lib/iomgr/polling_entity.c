/*
 *
 * Copyright 2016, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <grpc/support/alloc.h>
#include <grpc/support/log.h>

#include "src/core/lib/iomgr/polling_entity.h"

grpc_polling_entity grpc_polling_entity_create_from_pollset_set(
    grpc_pollset_set *pollset_set) {
  grpc_polling_entity pollent;
  pollent.pollent.pollset_set = pollset_set;
  pollent.tag = POPS_POLLSET_SET;
  return pollent;
}

grpc_polling_entity grpc_polling_entity_create_from_pollset(
    grpc_pollset *pollset) {
  grpc_polling_entity pollent;
  pollent.pollent.pollset = pollset;
  pollent.tag = POPS_POLLSET;
  return pollent;
}

grpc_pollset *grpc_polling_entity_pollset(grpc_polling_entity *pollent) {
  if (pollent->tag == POPS_POLLSET) {
    return pollent->pollent.pollset;
  }
  return NULL;
}

grpc_pollset_set *grpc_polling_entity_pollset_set(
    grpc_polling_entity *pollent) {
  if (pollent->tag == POPS_POLLSET_SET) {
    return pollent->pollent.pollset_set;
  }
  return NULL;
}

bool grpc_polling_entity_is_empty(const grpc_polling_entity *pollent) {
  return pollent->tag == POPS_NONE;
}

void grpc_polling_entity_add_to_pollset_set(grpc_exec_ctx *exec_ctx,
                                            grpc_polling_entity *pollent,
                                            grpc_pollset_set *pss_dst) {
  if (pollent->tag == POPS_POLLSET) {
    GPR_ASSERT(pollent->pollent.pollset != NULL);
    grpc_pollset_set_add_pollset(exec_ctx, pss_dst, pollent->pollent.pollset);
  } else if (pollent->tag == POPS_POLLSET_SET) {
    GPR_ASSERT(pollent->pollent.pollset_set != NULL);
    grpc_pollset_set_add_pollset_set(exec_ctx, pss_dst,
                                     pollent->pollent.pollset_set);
  } else {
    gpr_log(GPR_ERROR, "Invalid grpc_polling_entity tag '%d'", pollent->tag);
    abort();
  }
}

void grpc_polling_entity_del_from_pollset_set(grpc_exec_ctx *exec_ctx,
                                              grpc_polling_entity *pollent,
                                              grpc_pollset_set *pss_dst) {
  if (pollent->tag == POPS_POLLSET) {
    GPR_ASSERT(pollent->pollent.pollset != NULL);
    grpc_pollset_set_del_pollset(exec_ctx, pss_dst, pollent->pollent.pollset);
  } else if (pollent->tag == POPS_POLLSET_SET) {
    GPR_ASSERT(pollent->pollent.pollset_set != NULL);
    grpc_pollset_set_del_pollset_set(exec_ctx, pss_dst,
                                     pollent->pollent.pollset_set);
  } else {
    gpr_log(GPR_ERROR, "Invalid grpc_polling_entity tag '%d'", pollent->tag);
    abort();
  }
}

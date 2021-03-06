/*
 *
 * Copyright 2015, Google Inc.
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

#include "src/core/transport/chttp2/frame_goaway.h"

#include <string.h>

#include <grpc/support/alloc.h>
#include <grpc/support/log.h>

void grpc_chttp2_goaway_parser_init(grpc_chttp2_goaway_parser *p) {
  p->debug_data = NULL;
}

void grpc_chttp2_goaway_parser_destroy(grpc_chttp2_goaway_parser *p) {
  gpr_free(p->debug_data);
}

grpc_chttp2_parse_error grpc_chttp2_goaway_parser_begin_frame(
    grpc_chttp2_goaway_parser *p, gpr_uint32 length, gpr_uint8 flags) {
  if (length < 8) {
    gpr_log(GPR_ERROR, "goaway frame too short (%d bytes)", length);
    return GRPC_CHTTP2_CONNECTION_ERROR;
  }

  gpr_free(p->debug_data);
  p->debug_length = length - 8;
  p->debug_data = gpr_malloc(p->debug_length);
  p->debug_pos = 0;
  p->state = GRPC_CHTTP2_GOAWAY_LSI0;
  return GRPC_CHTTP2_PARSE_OK;
}

grpc_chttp2_parse_error grpc_chttp2_goaway_parser_parse(
    void *parser, grpc_chttp2_parse_state *state, gpr_slice slice,
    int is_last) {
  gpr_uint8 *const beg = GPR_SLICE_START_PTR(slice);
  gpr_uint8 *const end = GPR_SLICE_END_PTR(slice);
  gpr_uint8 *cur = beg;
  grpc_chttp2_goaway_parser *p = parser;

  switch (p->state) {
    case GRPC_CHTTP2_GOAWAY_LSI0:
      if (cur == end) {
        p->state = GRPC_CHTTP2_GOAWAY_LSI0;
        return GRPC_CHTTP2_PARSE_OK;
      }
      p->last_stream_id = ((gpr_uint32) * cur) << 24;
      ++cur;
    /* fallthrough */
    case GRPC_CHTTP2_GOAWAY_LSI1:
      if (cur == end) {
        p->state = GRPC_CHTTP2_GOAWAY_LSI1;
        return GRPC_CHTTP2_PARSE_OK;
      }
      p->last_stream_id |= ((gpr_uint32) * cur) << 16;
      ++cur;
    /* fallthrough */
    case GRPC_CHTTP2_GOAWAY_LSI2:
      if (cur == end) {
        p->state = GRPC_CHTTP2_GOAWAY_LSI2;
        return GRPC_CHTTP2_PARSE_OK;
      }
      p->last_stream_id |= ((gpr_uint32) * cur) << 8;
      ++cur;
    /* fallthrough */
    case GRPC_CHTTP2_GOAWAY_LSI3:
      if (cur == end) {
        p->state = GRPC_CHTTP2_GOAWAY_LSI3;
        return GRPC_CHTTP2_PARSE_OK;
      }
      p->last_stream_id |= ((gpr_uint32) * cur);
      ++cur;
    /* fallthrough */
    case GRPC_CHTTP2_GOAWAY_ERR0:
      if (cur == end) {
        p->state = GRPC_CHTTP2_GOAWAY_ERR0;
        return GRPC_CHTTP2_PARSE_OK;
      }
      p->error_code = ((gpr_uint32) * cur) << 24;
      ++cur;
    /* fallthrough */
    case GRPC_CHTTP2_GOAWAY_ERR1:
      if (cur == end) {
        p->state = GRPC_CHTTP2_GOAWAY_ERR1;
        return GRPC_CHTTP2_PARSE_OK;
      }
      p->error_code |= ((gpr_uint32) * cur) << 16;
      ++cur;
    /* fallthrough */
    case GRPC_CHTTP2_GOAWAY_ERR2:
      if (cur == end) {
        p->state = GRPC_CHTTP2_GOAWAY_ERR2;
        return GRPC_CHTTP2_PARSE_OK;
      }
      p->error_code |= ((gpr_uint32) * cur) << 8;
      ++cur;
    /* fallthrough */
    case GRPC_CHTTP2_GOAWAY_ERR3:
      if (cur == end) {
        p->state = GRPC_CHTTP2_GOAWAY_ERR3;
        return GRPC_CHTTP2_PARSE_OK;
      }
      p->error_code |= ((gpr_uint32) * cur);
      ++cur;
    /* fallthrough */
    case GRPC_CHTTP2_GOAWAY_DEBUG:
      memcpy(p->debug_data + p->debug_pos, cur, end - cur);
      p->debug_pos += end - cur;
      p->state = GRPC_CHTTP2_GOAWAY_DEBUG;
      if (is_last) {
        state->goaway = 1;
        state->goaway_last_stream_index = p->last_stream_id;
        state->goaway_error = p->error_code;
        state->goaway_text =
            gpr_slice_new(p->debug_data, p->debug_length, gpr_free);
        p->debug_data = NULL;
      }
      return GRPC_CHTTP2_PARSE_OK;
  }
  gpr_log(GPR_ERROR, "Should never end up here");
  abort();
  return GRPC_CHTTP2_CONNECTION_ERROR;
}

void grpc_chttp2_goaway_append(gpr_uint32 last_stream_id, gpr_uint32 error_code,
                               gpr_slice debug_data,
                               gpr_slice_buffer *slice_buffer) {
  gpr_slice header = gpr_slice_malloc(9 + 4 + 4);
  gpr_uint8 *p = GPR_SLICE_START_PTR(header);
  gpr_uint32 frame_length = 4 + 4 + GPR_SLICE_LENGTH(debug_data);

  /* frame header: length */
  *p++ = frame_length >> 16;
  *p++ = frame_length >> 8;
  *p++ = frame_length;
  /* frame header: type */
  *p++ = GRPC_CHTTP2_FRAME_GOAWAY;
  /* frame header: flags */
  *p++ = 0;
  /* frame header: stream id */
  *p++ = 0;
  *p++ = 0;
  *p++ = 0;
  *p++ = 0;
  /* payload: last stream id */
  *p++ = last_stream_id >> 24;
  *p++ = last_stream_id >> 16;
  *p++ = last_stream_id >> 8;
  *p++ = last_stream_id;
  /* payload: error code */
  *p++ = error_code >> 24;
  *p++ = error_code >> 16;
  *p++ = error_code >> 8;
  *p++ = error_code;
  GPR_ASSERT(p == GPR_SLICE_END_PTR(header));
  gpr_slice_buffer_add(slice_buffer, header);
  gpr_slice_buffer_add(slice_buffer, debug_data);
}

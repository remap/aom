/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#ifndef AOM_COMMON_IVFDEC_H_
#define AOM_COMMON_IVFDEC_H_

#include "common/tools_common.h"
#include "../av1/decoder/decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

int file_is_ivf(struct AvxInputContext *input);

/**
 * Do the work of file_is_ivf, once we have read the raw_hdr.
 * @param input A pointer to the struct AvxInputContext.
 * @param raw_hdr The raw header buffer.
 * @param nBytesRead The number of bytes read into raw_hdr (should be 32).
 * @return True if the header is for IVF.
 */
int file_is_ivf_raw_hdr
  (struct AvxInputContext *input, const char *raw_hdr, size_t nBytesRead);

typedef int64_t aom_codec_pts_t;
int ivf_read_frame(FILE *infile, uint8_t **buffer, size_t *bytes_read,
                   size_t *buffer_size, aom_codec_pts_t *pts);

typedef enum {
  PACKETIZER_MODE_NONE,
  PACKETIZER_MODE_READ_PACKETS,
  PACKETIZER_MODE_WRITE_PACKETS
} PacketizerMode;

struct PacketizerStruct;

typedef void (*Packetizer_WritePacketFunction)
  (struct PacketizerStruct *self, const char* nameSuffix, const uint8_t* content,
   size_t contentSize);

typedef int (*Packetizer_GetTileBuffersFunction)
  (struct PacketizerStruct *self, int tileGroupIndex, int nRows, int nColumns,
   TileBufferDec (*const tileBuffers)[MAX_TILE_COLS]);

/**
 * See the C++ class Packetizer for details.
 */
struct PacketizerStruct {
  int frameIndex;
  int tileGroupIndex;
  aom_codec_ctx_t codec;

  // This is only used if gPacketizerMode == PACKETIZER_MODE_WRITE_PACKETS.
  uint8_t nonTileContent[8000];
  size_t nonTileContentSize;
  Packetizer_WritePacketFunction writePacket;

  // This is only used if gPacketizerMode == PACKETIZER_MODE_READ_PACKETS.
  struct AvxInputContext input_ctx;
  Packetizer_GetTileBuffersFunction getTileBuffers;
};

typedef struct PacketizerStruct PacketizerStruct;

extern PacketizerMode gPacketizerMode;
extern struct PacketizerStruct *gPacketizer;

/**
 * Initialize the PacketizerStruct to default values, and set the global
 * variable gPacketizer if it is not already set.
 * @param self A pointer to the PacketizerStruct to initialize.
 */
static void
Packetizer_initialize(PacketizerStruct *self)
{
  self->frameIndex = -1;
  self->tileGroupIndex = -1;
  self->nonTileContentSize = 0;
  self->writePacket = NULL;
  self->getTileBuffers = NULL;
  self->codec.iface = NULL;
  self->codec.priv = NULL;

  if (!gPacketizer)
    // Set the global pointer.
    gPacketizer = self;
}

static void
Packetizer_finalize(PacketizerStruct *self)
{
  aom_codec_destroy(&self->codec);
  self->codec.iface = NULL;
  self->codec.priv = NULL;
}

/**
 * Append the data to self->nonTileContent. This is used if gPacketizerMode is
 * PACKETIZER_MODE_WRITE_PACKETS.
 * @param self A pointer to the PacketizerStruct to initialize.
 * @param data A pointer to the data buffer to append.
 * @param size The length of the data buffer.
 */
static void
Packetizer_appendNonTileContent
  (PacketizerStruct *self, const uint8_t* data, size_t size)
{
  if (self->nonTileContentSize + size > sizeof(self->nonTileContent))
    // We don't expect this to happen.
    return;

  memcpy(self->nonTileContent + self->nonTileContentSize, data, size);
  self->nonTileContentSize += size;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  // AOM_COMMON_IVFDEC_H_

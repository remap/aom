/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2019 Regents of the University of California.
 * @author: Jeff Thompson <jefft0@remap.ucla.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version, with the additional exemption that
 * compiling, linking, and/or using OpenSSL is allowed.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * A copy of the GNU Lesser General Public License is in the file COPYING.
 */

#ifndef NDN_PACKETIZER_HPP
#define NDN_PACKETIZER_HPP

#include "common/ivfdec.h"
#include <ndn-cpp/util/blob.hpp>

namespace av1 {

/**
 * The C++ Packetizer class extends the C PacketizerStruct to add methods for
 * packetizing and reading packets for AV! video.
 */
class Packetizer : public PacketizerStruct {
public:
  Packetizer()
  {
    construct();
  }

  ~Packetizer()
  {
    Packetizer_finalize(this);
  }

  /**
   * Set the global gPacketizerMode to PACKETIZER_MODE_WRITE_PACKETS to prepare
   * to write packets while reading the input AV1 file.
   */
  void
  startWrite()
  {
    gPacketizerMode = PACKETIZER_MODE_WRITE_PACKETS;
  }

  /**
   * Decode the file header to start reading a video and set up this->codec. On
   * return, this->input_ctx has the width and height. Note: This initially
   * calls Packetizer_finalize() and Packetizer_initialize() again to clear any
   * previous state.
   * @return True for success, false for error.
   */
  bool
  startRead(const uint8_t *fileHeader, size_t fileHeaderSize)
  {
    // Clear any previous state.
    Packetizer_finalize(this);
    construct();

    gPacketizerMode = PACKETIZER_MODE_READ_PACKETS;

    // Skip AvxVideoReader and aom_video_reader_open.
    if (!file_is_ivf_raw_hdr(&input_ctx, (const char *)fileHeader, fileHeaderSize))
      return false;

    const AvxInterface *decoder = NULL;
    decoder = get_aom_decoder_by_fourcc(input_ctx.fourcc);
    if (!decoder)
      return false;

    if (aom_codec_dec_init(&codec, decoder->codec_interface(), NULL, 0))
      return false;

    return true;
  }

  bool
  startRead(const ndn::Blob& fileHeader)
  {
    return startRead(fileHeader.buf(), fileHeader.size());
  }

  /**
   * Get the first tile group index for the frame, which is just a 4-byte
   * big-endian integer which comes before the video frame data.
   * @param nonTileData A pointer to the content of the non-tile Data packet.
   * @param nonTileDataSize The size of the nonTileData buffer, which must be at
   * least 4.
   * @return The first tile group index.
   */
  static uint32_t
  getFirstTileGroupIndex(const uint8_t *nonTileData, size_t nonTileDataSize)
  {
    if (nonTileDataSize < 4)
      // We don't expect this to happen.
      return 0xffffffff;

    return (((uint32_t)nonTileData[0]) << 24) +
           (((uint32_t)nonTileData[1]) << 16) +
           (((uint32_t)nonTileData[2]) << 8) +
            ((uint32_t)nonTileData[3]);
  }

  /**
   * Decode the frame and store the result in this->codec, which you can write
   * by calling writeFrame(). The non-tile packet starts with a 4-byte big
   * endian number of the first tile group index which will be used.
   * This will call the callback
   * getTileBuffers(tileGroupIndex, tileBuffers) for each tile group, where
   * tileGroupIndex is the tile group index and tileBuffers is a two-dimensional
   * array of TileBufferDec (which is simply a struct with a data pointer and
   * size). Your getTileBuffers should set tileBuffers[row][col] to the content
   * of the Data packet for the tiles that you wish to decode.
   * This should only be used after calling startRead() which sets the global
   * gPacketizerMode to PACKETIZER_MODE_READ_PACKETS
   * @param nonTileData A pointer to the content of the non-tile Data packet.
   * @param nonTileDataSize The size of the nonTileData buffer.
   * @return True for success, false for a decoding error.
   */
  bool
  decodeFrame(const uint8_t *nonTileData, size_t nonTileDataSize)
  {
    // A new frame.
    ++frameIndex;

    uint32_t nextTileGroupIndex = getFirstTileGroupIndex
      (nonTileData, nonTileDataSize);
    // tileGroupIndex is incremented before being used.
    tileGroupIndex = (int)nextTileGroupIndex - 1;

    // Ignore the 4-byte tile group index and the frame size info.
    size_t frameSize = nonTileDataSize - (4 + IVF_FRAME_HDR_SZ);
    const uint8_t* frame = nonTileData + 4 + IVF_FRAME_HDR_SZ;

    if (aom_codec_decode(&codec, frame, frameSize, NULL))
      return false;

    return true;
  }

  bool
  decodeFrame(const ndn::Blob& nonTileData)
  {
    return decodeFrame(nonTileData.buf(), nonTileData.size());
  }

  /**
   * Write the frame image that decodeFrame put in this->codec.
   * This should only be used after calling startRead() which sets the global
   * gPacketizerMode to PACKETIZER_MODE_READ_PACKETS
   * @param outFile The output FILE which should already be open for binary write.
   */
  void
  writeFrame(FILE* outFile)
  {
    aom_codec_iter_t iter = NULL;
    aom_image_t *img = NULL;
    while ((img = aom_codec_get_frame(&codec, &iter)) != NULL) {
      aom_img_write(img, outFile);
    }
  }

  /**
   * Your class should override this as described by startWrite().
   * @param nameSuffix The suffice of the Data packet name URI which your
   * method should append to the name prefix.
   * @param content A pointer to the Data packet content buffer.
   * @param contentSize The size of the content buffer.
   */
  virtual void
  writePacket
    (const char* nameSuffix, const uint8_t* content, size_t contentSize)
  {
  }

  /**
   * Your class should override this as described by decodeFrame().
   * @param tileGroupIndex The tile group index to use for fetching the correct
   * tile packets.
   * @param nRows The number of tile rows in the frame.
   * @param nColumns The number of tile columns in the frame.
   * @param tileBuffers A pointer to the array of tile buffers to be filled as
   * needed. When this is called, all [][].data are initialized to NULL.
   * @return True for success, false for error.
   */
  virtual bool
  getTileBuffers
    (int tileGroupIndex, int nRows, int nColumns,
     TileBufferDec (*const tileBuffers)[MAX_TILE_COLS])
  {
    return false;
  }

private:
  void
  construct()
  {
    Packetizer_initialize(this);
    PacketizerStruct::writePacket = writePacketFunction;
    PacketizerStruct::getTileBuffers = getTileBuffersFunction;
  }

  /**
   * Assume self is a pointer to a Packetizer and call its virtual writePacket().
   */
  static void writePacketFunction
    (struct PacketizerStruct *self, const char* nameSuffix,
     const uint8_t* content, size_t contentSize)
  {
    ((Packetizer*)self)->writePacket(nameSuffix, content, contentSize);
  }

  /**
   * Assume self is a pointer to a Packetizer and call its virtual getTileBuffers().
   */
  static int getTileBuffersFunction
    (struct PacketizerStruct *self, int tileGroupIndex, int nRows, int nColumns,
     TileBufferDec (*const tileBuffers)[MAX_TILE_COLS])
  {
    bool success = ((Packetizer*)self)->getTileBuffers
      (tileGroupIndex, nRows, nColumns, tileBuffers);
    return success ? 1 : 0;
  }
};

}

#endif

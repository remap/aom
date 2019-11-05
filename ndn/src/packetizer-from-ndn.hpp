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

#ifndef NDN_PACKETIZER_FROM_NDN_HPP
#define NDN_PACKETIZER_FROM_NDN_HPP

#include <set>
#include <utility>
#include <cnl-cpp/generalized-object/generalized-object-handler.hpp>
#include "packetizer.hpp"

namespace av1 {

/**
 * PacketizerFromNdn extends Packetizer so that we can override getTileBuffers
 * to assign only the tiles we want to decode. This has methods to fetch
 * generalized objects using the Common Name Library.
 */
class PacketizerFromNdn : public Packetizer {
public:
  /**
   * Create a PacketizerFromNdn to use the "nontile" and "tile" child
   * namespaces of the given prefixNamespace. To start, call
   * fetchFileHeaderAndStart(). When ready, this will write each decoded raw
   * frame to outFile. When finished, this sets enabled_ to false, so that you
   * can stop calling maybeDecodeFrame().
   * @param prefixNamespace The prefix Namespace with "nontile" and "tile"
   * children.
   * @param outFile The raw output video file, which should already be open.
   */
  PacketizerFromNdn(cnl_cpp::Namespace& prefixNamespace, FILE* outFile);

  /**
   * Fetch the "fileheader" generalized object and use it to call startRead().
   * Then call requestNewObjects() to begin fetching.
   */
  void
  fetchFileHeaderAndStart();

  /**
   * Call this periodically to check if we can decode the next needed frame.
   * If yes, then decode it and call requestNewObjects(). However, if
   * finalFrameIndex_ >= 0 (which was set after a timeout/nack) and this frame
   * is decoded, then set enabled_ = false, to quit.
   */
  void
  maybeDecodeFrame();

  /**
   * Override to read from NDN.
   */
  virtual bool getTileBuffers
    (int tileGroupIndex, int nRows, int nColumns,
     TileBufferDec (*const tileBuffers)[MAX_TILE_COLS]);

  // For frame index N, we must pre-fetch the tiles for tile groups up to
  // index N + tileGroupAdvance, since the frame may decode these in advance.
  static const int tileGroupAdvance = 5;

  // While processing frame N, we want outstanding interests for all nontile
  // objects up to frame N + framePipelineSize, and for all tile objects up to
  // N + framePipelineSize + tileGroupAdvance.
  static const int framePipelineSize = 30;

  // A set of the pair row,column .
  std::set<std::pair<int, int>> tileNumbers_;
  bool enabled_;

private:
  /**
   * Check if we have the nontile object and all the needed tile objects for
   * tile group indexes starting from startTileGroupIndex up to
   * startTileGroupIndex + tileGroupAdvance. However, if finalFrameIndex_ >= 0
   * (which was set after a timeout/nack) then only check up to that tile group
   * index.
   * @param startTileGroupIndex The starting tile group index to check.
   * @return True if we have all the needed tiles.
   */
  bool
  canDecodeFrame(int startTileGroupIndex);

  /**
   * Assume we need objects starting from N = frameIndex + 1. Request nontile
   * objects up to N + framePipelineSize and request tile objects up to
   * N + framePipelineSize + tileGroupAdvance. Update maxRequestedFrameIndex_
   * and maxRequestedTileGroupIndex_.
   * You should call this after calling decodeFrame(), which updates frameIndex
   * to the frame that was just processed.
   */
  void
  requestNewObjects();

  /**
   * This is called when there is a timeout/nack for a packet under the nontile
   * prefix, which we can use to determine finalFrameIndex_.
   */
  void
  onNontileStateChanged
    (cnl_cpp::Namespace& nameSpace, cnl_cpp::Namespace& changedNamespace,
     cnl_cpp::NamespaceState state, uint64_t callbackId);

  cnl_cpp::Namespace& prefixNamespace_;
  cnl_cpp::Namespace& nontileNamespace_;
  cnl_cpp::Namespace& tileNamespace_;
  FILE* outFile_;
  int finalFrameIndex_;
  int maxRequestedFrameIndex_;
  int maxRequestedTileGroupIndex_;
};

}

#endif

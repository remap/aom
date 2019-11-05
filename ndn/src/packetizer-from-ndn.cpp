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

#include "packetizer-from-ndn.hpp"

using namespace std;
using namespace ndn;
using namespace ndn::func_lib;
using namespace cnl_cpp;

namespace av1 {

PacketizerFromNdn::PacketizerFromNdn(Namespace& prefixNamespace, FILE* outFile)
: prefixNamespace_(prefixNamespace),
  nontileNamespace_(prefixNamespace[Name("nontile")[0]]),
  tileNamespace_(prefixNamespace[Name("tile")[0]]), outFile_(outFile),
  finalFrameIndex_(-1), maxRequestedFrameIndex_(-1),
  maxRequestedTileGroupIndex_(-1), enabled_(true)
{
  nontileNamespace_.addOnStateChanged
    (bind(&PacketizerFromNdn::onNontileStateChanged, this,
     _1, _2, _3, _4));
}

bool
PacketizerFromNdn::getTileBuffers
  (int tileGroupIndex, int nRows, int nColumns,
   TileBufferDec (*const tileBuffers)[MAX_TILE_COLS])
{
  if (tileNumbers_.size() == 0) {
    // Special case: We want all tiles but didn't know the number of tile rows
    // and columns until now. Just fill tileNumbers_. On return from
    // decodeFrame(), maybeDecodeFrame() will restart to fetch the tiles.
    for (int row = 0; row < nRows; ++row) {
      for (int col = 0; col < nColumns; ++col)
        tileNumbers_.insert(make_pair(row, col));
    }

    return true;
  }

  // Set the tiles as indicated by the tileNumbers_ list.
  for (set<pair<int, int>>::const_iterator i = tileNumbers_.begin();
       i != tileNumbers_.end(); ++i) {
    int row = i->first;
    int column = i->second;

    if (row < 0 || row >= nRows ||
        column < 0 || column >= nColumns)
      // Out of range.
      continue;

    char tileSuffix[256];
    sprintf(tileSuffix, "%d/%d/%d", tileGroupIndex, row, column);
    string tileUri = tileNamespace_.getName().toUri() + "/" + tileSuffix;
    Namespace& tile = tileNamespace_[Name(tileUri)];

    if (!tile.getObject()) {
      // We don't expect this. Just leave this tile blank.
      cout << "Error: No tile data for tile " << tile.getName() << endl;
      continue;
    }

    tileBuffers[row][column].data = tile.getBlobObject().buf();
    tileBuffers[row][column].size = tile.getBlobObject().size();
  }

  return true;
}

void
PacketizerFromNdn::fetchFileHeaderAndStart()
{
  auto onFileheaderObject = [&]
    (const ptr_lib::shared_ptr<ContentMetaInfoObject>& contentMetaInfo,
     Namespace& objectNamespace) {
    if (!startRead(objectNamespace.getBlobObject())) {
      cout << "fetchFileHeaderAndStart: Error is startRead()" << endl;
      return;
    }

    // Start fetching generalized object packets.
    requestNewObjects();
  };

  GeneralizedObjectHandler
    (&prefixNamespace_[Name("fileheader")[0]], onFileheaderObject).objectNeeded();
}

void
PacketizerFromNdn::maybeDecodeFrame()
{
  if (!canDecodeFrame(frameIndex + 1))
    return;

  size_t saveTileNumbersSize = tileNumbers_.size();
  Namespace& nontile = nontileNamespace_[Name(to_string(frameIndex + 1))[0]];
  if (!decodeFrame(nontile.getBlobObject())) {
    cout << "Failed to decode frame" << endl;
    return;
  }

  if (saveTileNumbersSize == 0) {
    // Special case: The user did not specify tile numbers because all tiles are
    // wanted. decodeFrame() called getTileBuffers() which filled tileNumbers_,
    // but now we need to restart and fetch the tiles.
    if (tileNumbers_.size() == 0) {
      // We don't expect this.
      cout << "tileNumbers_ is still empty after calling decodeFrame()" << endl;
      return;
    }

    Namespace& fileheader = prefixNamespace_[Name("fileheader")[0]];
    if (!startRead(fileheader.getBlobObject())) {
      cout << "Error is startRead" << endl;
      return;
    }

    // Now we can fetch the tiles.
    requestNewObjects();
    return;
  }

  writeFrame(outFile_);
  printf("\rProcessed frame %d", frameIndex);
  fflush(stdout);

  if (finalFrameIndex_ >= 0 && frameIndex == finalFrameIndex_) {
    // Finished decoding the video. Stop the process events loop.
    enabled_ = false;
    return;
  }

  // Now we can fetch more objects.
  requestNewObjects();
}

bool
PacketizerFromNdn::canDecodeFrame(int startTileGroupIndex)
{
  if (!nontileNamespace_[Name(to_string(startTileGroupIndex))[0]].getObject())
    // We don't have the nontile object.
    return false;

  if (tileNumbers_.size() == 0)
    // Special case: The user did not specify tile numbers because all tiles are
    // wanted. Return true so that maybeDecodeFrame() will call decodeFrame()
    // anyway.
    return true;

  int maxTileGroupIndex = startTileGroupIndex + tileGroupAdvance;
  if (finalFrameIndex_ >= 0) {
    if (finalFrameIndex_ < startTileGroupIndex)
      // We don't expect this.
      return true;
    if (finalFrameIndex_ < maxTileGroupIndex)
      // We know the final tile index, so don't check beyond that.
      maxTileGroupIndex = finalFrameIndex_;
  }

  for (int tileGroupIndex = startTileGroupIndex;
       tileGroupIndex <= maxTileGroupIndex; ++tileGroupIndex) {
    for (set<pair<int, int>>::const_iterator i = tileNumbers_.begin();
         i != tileNumbers_.end(); ++i) {
      int row = i->first;
      int column = i->second;

      char tileSuffix[256];
      sprintf(tileSuffix, "%d/%d/%d", tileGroupIndex, row, column);
      string tileUri = tileNamespace_.getName().toUri() + "/" + tileSuffix;
      Namespace& tile = tileNamespace_[Name(tileUri)];
      if (!tile.getObject())
        // We haven't received at least this tile.
        return false;
    }
  }

  return true;
}

void
PacketizerFromNdn::requestNewObjects()
{
  int targetFrameIndex = frameIndex + 1 + framePipelineSize;
  while (maxRequestedFrameIndex_ < targetFrameIndex) {
    ++maxRequestedFrameIndex_;
    Namespace& nontile = nontileNamespace_[Name(to_string(maxRequestedFrameIndex_))[0]];
    ptr_lib::make_shared<GeneralizedObjectHandler>(&nontile);
    nontile.objectNeeded();
  }

  if (tileNumbers_.size() == 0)
    // Special case: The user did not specify tile numbers because all tiles are
    // wanted. We have already requested the nontile objects. Return now so that
    // maybeDecodeFrame() will call decodeFrame() anyway.
    return;

  int targetTileGroupIndex = frameIndex + 1 + framePipelineSize + tileGroupAdvance;
  while (maxRequestedTileGroupIndex_ < targetTileGroupIndex) {
    ++maxRequestedTileGroupIndex_;

    for (set<pair<int, int>>::const_iterator i = tileNumbers_.begin();
         i != tileNumbers_.end(); ++i) {
      int row = i->first;
      int column = i->second;

      char tileSuffix[256];
      sprintf(tileSuffix, "%d/%d/%d", maxRequestedTileGroupIndex_, row, column);
      string tileUri = tileNamespace_.getName().toUri() + "/" + tileSuffix;
      Namespace& tile = tileNamespace_[Name(tileUri)];
      // Assume this object will persist, so we don't need shared_from_this().)
      ptr_lib::make_shared<GeneralizedObjectHandler>(&tile);
      tile.objectNeeded();
    }
  }
}

void
PacketizerFromNdn::onNontileStateChanged
  (Namespace& nameSpace, Namespace& changedNamespace, NamespaceState state,
   uint64_t callbackId) {
  if (state == NamespaceState_INTEREST_TIMEOUT ||
      state == NamespaceState_INTEREST_NETWORK_NACK) {
    // Get the index from the name.
    const Name::Component& indexComponent =
      changedNamespace.getName()[nontileNamespace_.getName().size()];
    int index = atoi(indexComponent.toEscapedString().c_str());
    if (index == 0) {
      cout << "Timeout/nack fetching the first frame " << changedNamespace.getName() << endl;
      enabled_ = false;
      return;
    }

    if (finalFrameIndex_ < 0)
      // finalFrameIndex_ is not set yet.
      finalFrameIndex_ = index - 1;
    else if (index - 1 < finalFrameIndex_)
      // This is an earlier timed-out frame, so reduce the finalFrameIndex_.
      finalFrameIndex_ = index - 1;
    // We may already have all the needed objects, so check.
    maybeDecodeFrame();
  }
}

}
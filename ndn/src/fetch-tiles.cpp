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

/**
 * Fetch NDN packets that were stored in the repo by store-tiles, and use them
 * to decode the AV1 video.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstring>

#include "aom/aom_decoder.h"
#include "common/tools_common.h"
#include "common/video_reader.h"
#include "packetizer.hpp"
#include <cnl-cpp/generalized-object/generalized-object-handler.hpp>

using namespace std;
using namespace av1;
using namespace ndn;
using namespace ndn::func_lib;
using namespace cnl_cpp;

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr, "Usage: %s <prefix> <outfile> [<row>,<col>] [<row>,<col>] ...\n", exec_name);
  exit(EXIT_FAILURE);
}

/**
 * PacketizerFromNdn extends Packetizer so that we can override getTileBuffers
 * to assign only the tiles we want to decode.
 */
class PacketizerFromNdn : public Packetizer {
public:
  /**
   * Create a PacketizerFromNdn to use the "nontile" and "tile" child
   * namespaces of the given prefixNamespace. When ready, write the decoded
   * raw frame to outFile.
   * @param prefixNamespace The prefix Namespace with "nontile" and "tile"
   * children.
   * @param outFile The raw output video file, which should already be open.
   */
  PacketizerFromNdn(Namespace& prefixNamespace, FILE* outFile)
  : nontileNamespace_(prefixNamespace[Name("nontile")[0]]),
    tileNamespace_(prefixNamespace[Name("tile")[0]]), outFile_(outFile),
    finalFrameIndex_(-1), maxRequestedFrameIndex_(-1),
    maxRequestedTileGroupIndex_(-1), enabled_(true)
  {
  }

  /**
   * Override to read from NDN.
   */
  virtual bool getTileBuffers
    (int tileGroupIndex, int nRows, int nColumns,
     TileBufferDec (*const tileBuffers)[MAX_TILE_COLS]);

  /**
   * Call this periodically to check if we can decode the next needed frame.
   * If yes, then decode it and call requestNewObjects(). However, if
   * finalFrameIndex_ >= 0 (which was set after a timeout/nack) and this frame
   * is decoded, then set enabled_ = false, to quit.
   */
  void
  maybeDecodeFrame();

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

  // For frame index N, we must pre-fetch the tiles for tile groups up to
  // index N + tileGroupAdvance, since the frame may decode these in advance.
  static const int tileGroupAdvance = 5;

  // While processing frame N, we want outstanding interests for all nontile
  // objects up to frame N + framePipelineSize, and for all tile objects up to
  // N + framePipelineSize + tileGroupAdvance.
  static const int framePipelineSize = 30;

  // A set of the pair row,column .
  set<pair<int, int>> tileNumbers_;
  int finalFrameIndex_;
  bool enabled_;

private:
  Namespace& nontileNamespace_;
  Namespace& tileNamespace_;
  FILE* outFile_;
  int maxRequestedFrameIndex_;
  int maxRequestedTileGroupIndex_;
};

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

    Namespace& prefixNamespace = *nontileNamespace_.getParent();
    Namespace& fileheader = prefixNamespace[Name("fileheader")[0]];
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

int main(int argc, char **argv) {
  // Silence the warning from Interest wire encode.
  Interest::setDefaultCanBePrefix(true);

  exec_name = argv[0];

  if (argc < 3)
    die("Invalid number of arguments.");

  FILE *outFile = outFile = fopen(argv[2], "wb");
  if (!outFile) {
    die("Failed to open %s for writing.\n", argv[2]);
    return 1;
  }

  Face face;
  Name prefix(argv[1]);
  cout << "Begin fetching video " << prefix << endl;
  Namespace prefixNamespace(prefix);
  prefixNamespace.setFace(&face);
  PacketizerFromNdn packetizer(prefixNamespace, outFile);

  // The remaining args are tile numbers of format <row>,<col>
  for (int i = 3; i < argc; ++i) {
    char* row_col = argv[i];
    char* comma = strchr(row_col, ',');
    if (!comma)
      die("Can't find the comma in <row>,<col> \"%s\"\n", row_col);

    int row = atoi(string(row_col, comma).c_str());
    int col = atoi(string(comma + 1, row_col + strlen(row_col)).c_str());

    packetizer.tileNumbers_.insert(make_pair(row, col));
  }
  // Note: Special case if tileNumbers_ is empty, meaning we want all tiles:
  // At first we only fetch the nontile frame info but no tiles. getTileBuffers()
  // will get the number of tile rows and columns, and maybeDecodeFrame() will
  // restart to get the tiles.

  Namespace& nontileNamespace = prefixNamespace[Name("nontile")[0]];

  // We're finished when there is a timeout/nack in fetching a nontile packet.
  auto onStateChanged = [&]
    (Namespace& nameSpace, Namespace& changedNamespace, NamespaceState state,
     uint64_t callbackId) {
    if ((state == NamespaceState_INTEREST_TIMEOUT ||
         state == NamespaceState_INTEREST_NETWORK_NACK) &&
        nontileNamespace.getName().isPrefixOf(changedNamespace.getName())) {
      // Get the index from the name.
      const Name::Component& indexComponent =
        changedNamespace.getName()[nontileNamespace.getName().size()];
      int index = atoi(indexComponent.toEscapedString().c_str());
      if (index == 0) {
        cout << "Timeout/nack fetching the first frame " << changedNamespace.getName() << endl;
        packetizer.enabled_ = false;
        return;
      }

      if (packetizer.finalFrameIndex_ < 0)
        // finalFrameIndex_ is not set yet.
        packetizer.finalFrameIndex_ = index - 1;
      else if (index - 1 < packetizer.finalFrameIndex_)
        // This is an earlier timed-out frame, so reduce the finalFrameIndex_.
        packetizer.finalFrameIndex_ = index - 1;
      // We may already have all the needed objects, so check.
      packetizer.maybeDecodeFrame();
    }
  };
  prefixNamespace.addOnStateChanged(onStateChanged);

  auto onFileheaderObject = [&]
    (const ptr_lib::shared_ptr<ContentMetaInfoObject>& contentMetaInfo,
     Namespace& objectNamespace) {
    if (!packetizer.startRead(objectNamespace.getBlobObject())) {
      cout << "Error is startRead" << endl;
      return;
    }

    // Start fetching nontile.
    packetizer.requestNewObjects();
  };

  GeneralizedObjectHandler
    (&prefixNamespace[Name("fileheader")[0]], onFileheaderObject).objectNeeded();

  while (packetizer.enabled_) {
    // Check if we have received the needed packets, and decode.
    packetizer.maybeDecodeFrame();

    face.processEvents();
    // We need to sleep for a few milliseconds so we don't use 100% of the CPU.
    usleep(10000);
  }

  int framerate = (int)((double)packetizer.input_ctx.framerate.numerator /
                        (double)packetizer.input_ctx.framerate.denominator);
  printf("\nPlay: ffplay -f rawvideo -pix_fmt yuv420p -s %dx%d -framerate %d %s\n",
         packetizer.input_ctx.width, packetizer.input_ctx.height, framerate,
         argv[2]);

  fclose(outFile);

  return EXIT_SUCCESS;
}

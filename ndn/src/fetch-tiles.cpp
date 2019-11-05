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
 * Fetch NDN generalized objects that were stored in the repo by store-tiles,
 * and use them to decode the AV1 video.
 */

#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include "packetizer-from-ndn.hpp"

using namespace std;
using namespace av1;
using namespace ndn;
using namespace cnl_cpp;

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr, "Usage: %s <prefix> <outfile> [<row>,<col>] [<row>,<col>] ...\n", exec_name);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
  // Silence the warning from Interest wire encode.
  Interest::setDefaultCanBePrefix(true);

  exec_name = argv[0];

  if (argc < 3)
    die("Invalid number of arguments.");

  FILE *outFile = outFile = fopen(argv[2], "wb");
  if (!outFile)
    die("Failed to open %s for writing.\n", argv[2]);

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

  packetizer.fetchFileHeaderAndStart();

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

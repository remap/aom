/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */

/**
 * Imitate aom/examples/simple_decoder, but use Packetizer.startWrite() so that
 * the decoder calls writePacket for each part of the AV1 file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

#include "aom/aom_decoder.h"
#include "common/tools_common.h"
#include "common/video_reader.h"
#include "common/ivfdec.h"
#include "packetizer.hpp"
#include <cnl-cpp/generalized-object/generalized-object-handler.hpp>
#include <storage/storage-engine.hpp>

using namespace std;
using namespace av1;
using namespace ndn;
using namespace cnl_cpp;
using namespace fast_repo;

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr, "Usage: %s <infile> <prefix> [<path_to_db>]\n", exec_name);
  exit(EXIT_FAILURE);
}

/**
 * PacketizerToRepo extends Packetizer so that we can override writePacket() to
 * store Data packets to the repo.
 */
class PacketizerToRepo : public Packetizer {
public:
  /**
   * Create a PacketizerToRepo, configuring writePacket to store to the repo.
   * @param prefixNamespace The CNL Namespace used to create the generalized
   * object.
   * @param storageEngine This calls storageEngine.put for each Data packet of
   * the generalized object.
   */
  PacketizerToRepo(Namespace& prefixNamespace, StorageEngine& storageEngine)
  : prefixNamespace_(prefixNamespace), storageEngine_(storageEngine)
  {
  }

  /**
   * Override to write to a the repo.
   */
  virtual void
  writePacket
    (const char* nameSuffix, const uint8_t* content, size_t contentSize)
  {
    // Create the generalized object.
    string uri = prefixNamespace_.getName().toUri() + "/" + nameSuffix;
    Namespace& objectNamespace = prefixNamespace_[Name(uri)];
    handler_.setObject
      (objectNamespace, Blob(content, contentSize), "application/binary");

    // Get all the created Data packets and store in the repo.
    vector<ptr_lib::shared_ptr<Data>> dataList;
    objectNamespace.getAllData(dataList);
    for (size_t i = 0; i < dataList.size(); ++i)
      storageEngine_.put(dataList[i]);
  }

  Namespace& prefixNamespace_;
  StorageEngine& storageEngine_;
  GeneralizedObjectHandler handler_;
};

int main(int argc, char **argv) {
  exec_name = argv[0];

  if (argc < 3 || argc > 4)
    die("Invalid number of arguments.");

  string dbPath;
  if (argc == 4)
    dbPath = argv[3];
  else
    dbPath = "/var/db/fast-repo";
  StorageEngine storageEngine(dbPath);

  KeyChain keyChain;
  Name prefix(argv[2]);
  Namespace prefixNamespace(prefix, &keyChain);
  PacketizerToRepo packetizer(prefixNamespace, storageEngine);
  packetizer.startWrite();

   AvxVideoReader *reader = aom_video_reader_open(argv[1]);
  if (!reader) die("Failed to open %s for reading.", argv[1]);

  const AvxVideoInfo *info = aom_video_reader_get_info(reader);

  const AvxInterface *decoder = get_aom_decoder_by_fourcc(info->codec_fourcc);
  if (!decoder) die("Unknown input codec.");

  if (aom_codec_dec_init(&packetizer.codec, decoder->codec_interface(), NULL, 0))
    die_codec(&packetizer.codec, "Failed to initialize decoder.");

  cout << "Storing video " << prefix.toUri() << endl;
  while (aom_video_reader_read_frame(reader)) {
    size_t frame_size = 0;
    const unsigned char *frame =
        aom_video_reader_get_frame(reader, &frame_size);
    if (aom_codec_decode(&packetizer.codec, frame, frame_size, NULL))
      die_codec(&packetizer.codec, "Failed to decode frame.");

    printf("\rProcessed frame %d", gPacketizer->frameIndex);
    fflush(stdout);
  }

  aom_video_reader_close(reader);

  // Note: ~Packetizer calls aom_codec_destroy(&packetizer.codec).

  printf("\nFinished.");
  return EXIT_SUCCESS;
}

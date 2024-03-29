//
// storage-engine.cpp
//
//  Created by Peter Gusev on 27 May 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include "storage-engine.hpp"

#include <unordered_map>
#include <ndn-cpp/data.hpp>
#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/name.hpp>
#include <ndn-cpp/util/blob.hpp>
#include <boost/algorithm/string.hpp>
#include <ndn-cpp/digest-sha256-signature.hpp>

//#include "../config.hpp"
#include "../../src/ndn-av1-config.h"
#if NDN_AV1_HAVE_LIBROCKSDB
#define HAVE_LIBROCKSDB 1
#else
#define HAVE_LIBROCKSDB 0
#endif

#if HAVE_LIBROCKSDB

#ifndef __ANDROID__ // use RocksDB on linux and macOS

#include <rocksdb/db.h>
namespace db_namespace = rocksdb;

#else // for Android - use LevelDB

#include <leveldb/db.h>
namespace db_namespace = leveldb;

#endif

#endif

using namespace fast_repo;
using namespace ndn;
using namespace boost;

//******************************************************************************
namespace fast_repo
{

class StorageEngineImpl : public std::enable_shared_from_this<StorageEngineImpl>
{
  public:
    typedef struct _Stats
    {
        size_t nKeys_;
        size_t valueSizeBytes_;
    } Stats;

#if HAVE_LIBROCKSDB
    StorageEngineImpl(std::string dbPath) : dbPath_(dbPath), db_(nullptr), keysTrieBuilt_(false)
    {
    }
#else
    StorageEngineImpl(std::string dbPath)
    {
        throw std::runtime_error("The library is not copmiled with persistent storage support.");
    }
#endif

    ~StorageEngineImpl()
    {
        close();
    }

    bool open(bool readOnly);
    void close();
    void setRenamePrefix(const std::string& p){ renamePrefix_ = p; }
    std::string getRenamePrefix()  { return renamePrefix_; }

    Name put(const Data &data);
    std::shared_ptr<Data> get(const Name &dataName);
    std::shared_ptr<Data> read(const Interest &interest);

    void getLongestPrefixes(asio::io_service &io,
                            function<void(const std::vector<Name> &)> onCompletion);
    const Stats &getStats() const { return stats_; }

  private:
    class NameTrie
    {
      public:
        struct TrieNode
        {
            bool isLeaf;
            std::unordered_map<std::string, std::shared_ptr<TrieNode>> components;

            TrieNode() : isLeaf(false) {}
        };

        NameTrie() : head_(std::make_shared<TrieNode>()) {}

        void insert(const std::string &n)
        {

            std::shared_ptr<TrieNode> curr = head_;
            std::vector<std::string> components;
            split(components, n, boost::is_any_of("/"));

            for (auto c : components)
            {
                if (c.size() == 0)
                    continue;
                if (curr->components.find(c) == curr->components.end())
                    curr->components[c] = std::make_shared<TrieNode>();
                curr = curr->components[c];
            }
        }

        // gets all longest prefixes
        const std::vector<Name> getLongestPrefixes() const
        {
            std::vector<Name> longestPrefixes;

            for (auto cIt : head_->components)
            {
                Name n(cIt.first);
                std::shared_ptr<TrieNode> curr = cIt.second;

                while (curr.get() && !curr->isLeaf && curr->components.size() == 1)
                {
                    auto it = curr->components.begin();
                    n.append(Name::fromEscapedString(it->first));
                    curr = it->second;
                }
                longestPrefixes.push_back(n);
            }

            return longestPrefixes;
        }

      private:
        std::shared_ptr<TrieNode> head_;
    };

    std::string dbPath_, renamePrefix_;
    bool keysTrieBuilt_;
    NameTrie keysTrie_;
    Stats stats_;
#if HAVE_LIBROCKSDB
    db_namespace::DB *db_;
#endif

    void buildKeyTrie();
};

} // namespace fast_repo

//******************************************************************************
StorageEngine::StorageEngine(std::string dbPath, bool readOnly, std::string renamePrefix) 
    : pimpl_(std::make_shared<StorageEngineImpl>(dbPath))
{
    try
    {
        pimpl_->open(readOnly);
        
        if (renamePrefix != "")
            pimpl_->setRenamePrefix(renamePrefix);
    }
    catch (std::exception &e)
    {
        throw std::runtime_error("Failed to open storage at " + dbPath + ": " + e.what());
    }
}

StorageEngine::~StorageEngine()
{
    pimpl_->close();
}

Name StorageEngine::put(const std::shared_ptr<const Data> &data)
{
    Name n = pimpl_->put(*data);
    this->afterDataInsertion(data->getName());
    return n;
}

Name StorageEngine::put(const Data &data)
{
    Name n = pimpl_->put(data);
    this->afterDataInsertion(data.getName());
    return n;
}

std::shared_ptr<Data>
StorageEngine::get(const Name &dataName)
{
    return pimpl_->get(dataName);
}

std::shared_ptr<Data>
StorageEngine::read(const Interest &interest)
{
    return pimpl_->read(interest);
}

void StorageEngine::scanForLongestPrefixes(asio::io_service &io,
                                           function<void(const std::vector<ndn::Name> &)> onCompleted)
{
    pimpl_->getLongestPrefixes(io, onCompleted);
}

const size_t
StorageEngine::getPayloadSize() const
{
    return pimpl_->getStats().valueSizeBytes_;
}

const size_t
StorageEngine::getKeysNum() const
{
    return pimpl_->getStats().nKeys_;
}

std::string 
StorageEngine::getRenamePrefix() const
{
    return pimpl_->getRenamePrefix();
}

//******************************************************************************
bool StorageEngineImpl::open(bool readOnly)
{
#if HAVE_LIBROCKSDB
    db_namespace::Options options;
    options.create_if_missing = true;
    db_namespace::Status status;
    if (readOnly)
        status = db_namespace::DB::OpenForReadOnly(options, dbPath_, &db_);
    else
        status = db_namespace::DB::Open(options, dbPath_, &db_);

    if (!status.ok())
        throw std::runtime_error(status.getState());

    return status.ok();
#else
    return false;
#endif
}

void StorageEngineImpl::close()
{
#if HAVE_LIBROCKSDB
    if (db_)
    {
        // db_->SyncWAL();
        // db_->Close();
        delete db_;
        db_ = nullptr;
    }
#endif
}

Name StorageEngineImpl::put(const Data &data)
{ 
#if HAVE_LIBROCKSDB
    if (!db_)
        throw std::runtime_error("DB is not open");
    if (renamePrefix_ != "")
    {
        // let's rename data packet here
        Data d(Name(renamePrefix_).append(data.getName()));
        d.setMetaInfo(data.getMetaInfo());
        d.setContent(data.getContent());

        // add phony signature
        static uint8_t digest[ndn_SHA256_DIGEST_SIZE];
        memset(digest, 0, ndn_SHA256_DIGEST_SIZE);
        ndn::Blob signatureBits(digest, sizeof(digest));
        d.setSignature(ndn::DigestSha256Signature());
        ndn::DigestSha256Signature *sha256Signature = (ndn::DigestSha256Signature *)d.getSignature();
        sha256Signature->setSignature(signatureBits);
        
        db_namespace::Status s =
            db_->Put(db_namespace::WriteOptions(),
                 d.getName().toUri(),
                 db_namespace::Slice((const char *)d.wireEncode().buf(),
                                     d.wireEncode().size()));
        if (s.ok())
            return d.getName();
    }
    else
    {
        db_namespace::Status s =
            db_->Put(db_namespace::WriteOptions(),
                 data.getName().toUri(),
                 db_namespace::Slice((const char *)data.wireEncode().buf(),
                                     data.wireEncode().size()));
        if (s.ok())
            return data.getName();
    }
#endif
    return Name();
}

std::shared_ptr<Data> StorageEngineImpl::get(const Name &dataName)
{
#if HAVE_LIBROCKSDB
    if (!db_)
        throw std::runtime_error("DB is not open");

    static std::string dataString;
    db_namespace::Status s = db_->Get(db_namespace::ReadOptions(),
                                      dataName.toUri(),
                                      &dataString);
    if (s.ok())
    {
        std::shared_ptr<Data> data = std::make_shared<Data>();
        data->wireDecode((const uint8_t *)dataString.data(), dataString.size());

        return data;
    }
#endif
    return std::shared_ptr<Data>(nullptr);
}

std::shared_ptr<Data> StorageEngineImpl::read(const Interest &interest)
{
    std::shared_ptr<Data> data;
    // NOTE: Why isn't getCanBePrefix() a const function?
    bool canBePrefix = const_cast<Interest&>(interest).getCanBePrefix();

    if (canBePrefix)
    {
        // TODO: implement prefix match data retrieval
        // extract by prefix match
        Name prefix = interest.getName(), keyName;
        auto it = db_->NewIterator(db_namespace::ReadOptions());
        std::string key = "";
        bool checkMaxSuffixComponents = interest.getMaxSuffixComponents() != -1;
        bool checkMinSuffixComponents = interest.getMinSuffixComponents() != -1;

        for (it->Seek(prefix.toUri());
             it->Valid() && it->key().starts_with(prefix.toUri());
             it->Next())
        {
            if (checkMaxSuffixComponents || checkMinSuffixComponents)
            {
                keyName = Name(it->key().ToString());
                int nSuffixComponents = keyName.size() - prefix.size();
                bool passCheck = false;

                if (checkMaxSuffixComponents && 
                    nSuffixComponents <= interest.getMaxSuffixComponents())
                    passCheck = true;
                if (checkMinSuffixComponents &&
                    nSuffixComponents >= interest.getMinSuffixComponents())
                    passCheck = true;
                
                if (passCheck)
                    key = it->key().ToString();
            }
            else
                key = it->key().ToString();
        }

        if (key != "")
            data = get(Name(key));

        delete it;
    }
    else
        data =  get(interest.getName());

    return data;
}

void StorageEngineImpl::getLongestPrefixes(asio::io_service &io,
                                           function<void(const std::vector<Name> &)> onCompletion)
{
    // if (!keysTrieBuilt_)
    //     onCompletion(keysTrie_.getLongestPrefixes());
    // else
    // {
    std::shared_ptr<StorageEngineImpl> me = shared_from_this();
    io.dispatch([me, this, onCompletion]() {
        buildKeyTrie();
        keysTrieBuilt_ = true;
        onCompletion(keysTrie_.getLongestPrefixes());
    });
    // }
}

void StorageEngineImpl::buildKeyTrie()
{
    stats_.nKeys_ = 0;
    stats_.valueSizeBytes_ = 0;
#if HAVE_LIBROCKSDB

    db_namespace::Iterator *it = db_->NewIterator(rocksdb::ReadOptions());

    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        keysTrie_.insert(it->key().ToString());
        stats_.nKeys_++;
        stats_.valueSizeBytes_ += it->value().size();
    }
    assert(it->status().ok()); // Check for any errors found during the scan

    delete it;
#endif
}
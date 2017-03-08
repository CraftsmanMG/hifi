//
//  KTXCache.h
//  libraries/model-networking/src
//
//  Created by Zach Pomerantz 2/22/2017.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_KTXCache_h
#define hifi_KTXCache_h

#include <QUrl>

#include <FileCache.h>

namespace ktx {
    class KTX;
}

class KTXFile;
using KTXFilePointer = std::shared_ptr<KTXFile>;

class KTXCache : public cache::FileCache {
    Q_OBJECT

public:
    KTXCache(const std::string& dir, const std::string& ext) : FileCache(dir, ext) { initialize(); }

    struct Data {
        Data(const QUrl& url, const Key& key, const char* data, size_t length) :
            url(url), key(key), data(data), length(length) {}
        const QUrl url;
        const Key key;
        const char* data;
        size_t length;
    };

    KTXFilePointer writeFile(Data data);
    KTXFilePointer getFile(const QUrl& url);

protected:
    std::unique_ptr<cache::File> createFile(const Key& key, const std::string& filepath, size_t length, void* extra) override final;
    std::unique_ptr<cache::File> loadFile(const Key& key, const std::string& filepath, size_t length, const std::string& metadata) override final;
    void evictedFile(const cache::FilePointer& file) override final;

private:
    std::unique_ptr<cache::File> createKTXFile(const Key& key, const std::string& filepath, size_t length, const QUrl& url);

    using Mutex = std::mutex;
    using Lock = std::lock_guard<Mutex>;
    struct QUrlHasher { std::size_t operator()(QUrl const& url) const { return qHash(url); } };

    std::unordered_map<QUrl, Key, QUrlHasher> _urlMap;
    Mutex _urlMutex;
};

class KTXFile : public cache::File {
    Q_OBJECT

public:
    QUrl getUrl() const { return _url; }
    std::unique_ptr<ktx::KTX> getKTX() const;

protected:
    KTXFile(const Key& key, const std::string& filepath, size_t length, const QUrl& url) :
        File(key, filepath, length), _url(url) {}

    std::string getMetadata() const override final;

private:
    friend class KTXCache;

    const QUrl _url;
};

#endif // hifi_KTXCache_h
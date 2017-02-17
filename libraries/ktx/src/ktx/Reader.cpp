//
//  Reader.cpp
//  ktx/src/ktx
//
//  Created by Zach Pomerantz on 2/08/2017.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "KTX.h"

#include <list>
#include <QtGlobal>

namespace ktx {
    class ReaderException: public std::exception {
    public:
        ReaderException(const std::string& explanation) : _explanation("KTX deserialization error: " + explanation) {}
        const char* what() const override {
            return _explanation.c_str();
        }
    private:
        const std::string _explanation;
    };

    bool checkEndianness(uint32_t endianness, bool& matching) {
        switch (endianness) {
        case Header::ENDIAN_TEST: {
            matching = true;
            return true;
            }
            break;
        case Header::REVERSE_ENDIAN_TEST:
            {
                matching = false;
                return true;
            }
            break;
        default:
            return false;
        }
    }

    bool checkIdentifier(const Byte* identifier) {
        return memcmp(identifier, Header::IDENTIFIER.data(), Header::IDENTIFIER_LENGTH);
    }

    KeyValues getKeyValues(size_t length, const Byte* src) {
        KeyValues keyValues;
        size_t offset = 0;

        while (offset < length) {
            // determine byte size
            uint32_t keyValueByteSize;
            memcpy(&keyValueByteSize, src, sizeof(uint32_t));
            if (keyValueByteSize > length - offset) {
                throw ReaderException("invalid key-value size");
            }

            // find the first null character \0
            int keyLength = 0;
            while (reinterpret_cast<const char*>(src[++keyLength]) != '\0') {
                if (keyLength == keyValueByteSize) {
                    // key must be null-terminated, and there must be space for the value
                    throw ReaderException("invalid key-value " + std::string(reinterpret_cast<const char*>(src), keyLength));
                }
            }

            // populate the key-value
            keyValues.emplace_back(
                std::move(std::string(reinterpret_cast<const char*>(src), keyLength)),
                std::move(std::string(reinterpret_cast<const char*>(src + keyLength), keyValueByteSize - keyLength)));

            // advance offset/src
            uint32_t keyValuePadding = 3 - ((keyValueByteSize + 3) % PACKING_SIZE);
            offset += keyValueByteSize + keyValuePadding;
            src += keyValueByteSize + keyValuePadding;
        }

        return keyValues;
    }

    Images getImagesTable(const Header& header, size_t mipsDataSize, const Byte* mipsData) {
        Images images;
        auto currentPtr = mipsData;
        auto numMips = header.getNumberOfLevels();

        // Keep identifying new mip as long as we can at list query the next imageSize
        while ((currentPtr - mipsData) + sizeof(uint32_t) <= (mipsDataSize)) {

            // Grab the imageSize coming up
            size_t imageSize = *reinterpret_cast<const uint32_t*>(currentPtr);
            currentPtr += sizeof(uint32_t);

            // If enough data ahead then capture the pointer
            if ((currentPtr - mipsData) + imageSize <= (mipsDataSize)) {
                auto padding = Header::evalPadding(imageSize);

                images.emplace_back(Image(imageSize, padding, currentPtr));

                currentPtr += imageSize + padding;
            } else {
                break;
            }
        }

        return images;
    }


    bool KTX::checkHeaderFromStorage(const Storage& src) {
        try {
            auto srcSize = src.size();
            auto srcBytes = src.data();

            // validation
            if (srcSize < sizeof(Header)) {
                throw ReaderException("length is too short for header");
            }
            const Header* header = reinterpret_cast<const Header*>(srcBytes);

            if (!checkIdentifier(header->identifier)) {
                throw ReaderException("identifier field invalid");
            }

            bool endianMatch { true };
            if (!checkEndianness(header->endianness, endianMatch)) {
                throw ReaderException("endianness field has invalid value");
            }

            // TODO: endian conversion if !endianMatch - for now, this is for local use and is unnecessary


            // TODO: calculated bytesOfTexData
            if (srcSize < (sizeof(Header) + header->bytesOfKeyValueData)) {
                throw ReaderException("length is too short for metadata");
            }

             size_t bytesOfTexData = 0;
            if (srcSize < (sizeof(Header) + header->bytesOfKeyValueData + bytesOfTexData)) {

                throw ReaderException("length is too short for data");
            }

            return true;
        }
        catch (ReaderException& e) {
            qWarning(e.what());
            return false;
        }
    }

    std::unique_ptr<KTX> KTX::create(const Storage& src) {
        auto srcCopy = std::make_unique<Storage>(src);

        return create(srcCopy);
    }

    std::unique_ptr<KTX> KTX::create(std::unique_ptr<Storage>& src) {
        if (!src) {
            return nullptr;
        }

        try {
            if (!checkHeaderFromStorage(*src)) {

            }

            std::unique_ptr<KTX> result(new KTX());
            result->resetStorage(src.release());

            // read metadata
            result->_keyValues = getKeyValues(result->getHeader()->bytesOfKeyValueData, result->getKeyValueData());

            // populate image table
            result->_images = getImagesTable(*result->getHeader(), result->getTexelsDataSize(), result->getTexelsData());

            return result;
        }
        catch (ReaderException& e) {
            qWarning(e.what());
            return nullptr;
        }
    }
}
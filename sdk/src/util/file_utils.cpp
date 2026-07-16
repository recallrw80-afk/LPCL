#include "util/file_utils.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDataStream>
#include <zlib.h>

namespace FileUtils {

QString assetPathFromHash(const QString &hash) {
    return hash.left(2) + "/" + hash;
}

bool verifySha1(const QString &filePath, const QString &expectedHash) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QCryptographicHash hasher(QCryptographicHash::Sha1);
    hasher.addData(&file);
    file.close();

    QString actual = hasher.result().toHex().toLower();
    return actual == expectedHash.toLower();
}

// Minimal ZIP/JAR extractor for native libraries
// Extracts .so, .dll, .dylib, .jnilib files, skipping META-INF/
// Supports STORED (method 0) and DEFLATE (method 8) via zlib

static bool isNativeFile(const QString &name)
{
    if (name.startsWith("META-INF/")) return false;
    if (name.endsWith('/')) return false;
    return name.endsWith(".so") || name.endsWith(".dll") ||
           name.endsWith(".dylib") || name.endsWith(".jnilib");
}

static quint16 readU16(QDataStream &s)
{
    quint16 v;
    s.setByteOrder(QDataStream::LittleEndian);
    s >> v;
    return v;
}

static quint32 readU32(QDataStream &s)
{
    quint32 v;
    s.setByteOrder(QDataStream::LittleEndian);
    s >> v;
    return v;
}

/// Decompress raw DEFLATE data (no zlib/gzip header) using zlib
static QByteArray inflateRaw(const QByteArray &compressed, quint32 expectedSize)
{
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.constData()));
    strm.avail_in = compressed.size();

    // -MAX_WBITS = raw deflate (no header)
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK)
        return {};

    QByteArray result;
    result.resize(expectedSize > 0 ? expectedSize : compressed.size() * 4);
    if (result.isEmpty()) result.resize(4096);

    int ret;
    do {
        strm.next_out = reinterpret_cast<Bytef*>(result.data() + strm.total_out);
        strm.avail_out = result.size() - strm.total_out;

        if (strm.avail_out == 0) {
            result.resize(result.size() * 2);
            strm.avail_out = result.size() - strm.total_out;
            strm.next_out = reinterpret_cast<Bytef*>(result.data() + strm.total_out);
        }

        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&strm);
            return {};
        }
    } while (ret != Z_STREAM_END);

    result.resize(strm.total_out);
    inflateEnd(&strm);
    return result;
}

/// Skip compressed data when we don't need it, handling data descriptors correctly
static void skipCompressedData(QFile &file, QDataStream &stream, quint32 compressedSize, bool hasDataDescriptor)
{
    if (compressedSize > 0) {
        // Known size — skip directly
        file.seek(file.pos() + compressedSize);
    } else if (hasDataDescriptor) {
        // Unknown size — scan for data descriptor signature
        // Use overlapping reads to handle signature crossing chunk boundaries
        QByteArray carry;
        QByteArray buf(8192, Qt::Uninitialized);
        while (!stream.atEnd()) {
            qint64 bytesRead = stream.readRawData(buf.data(), buf.size());
            if (bytesRead <= 0) break;

            QByteArray search = carry + QByteArray::fromRawData(buf.data(), bytesRead);
            int searchLen = search.size();

            for (int i = 0; i <= searchLen - 4; ++i) {
                quint32 sig = static_cast<quint8>(search[i]) |
                              (static_cast<quint8>(search[i+1]) << 8) |
                              (static_cast<quint8>(search[i+2]) << 16) |
                              (static_cast<quint8>(search[i+3]) << 24);
                if (sig == 0x08074b50) {
                    // Found descriptor — skip signature (already consumed) + CRC(4) + compSize(4) + uncompSize(4) = 12 bytes
                    // But we may have read past it. Calculate how many bytes to seek back/forward.
                    int descriptorEnd = i + 4 + 12; // sig + 3 fields
                    int overread = searchLen - descriptorEnd;
                    if (overread > 0) {
                        file.seek(file.pos() - overread);
                    }
                    return;
                }
            }
            // Keep last 3 bytes for overlap detection
            if (searchLen > 3)
                carry = search.right(3);
            else
                carry = search;
        }
    }
    // If neither known size nor data descriptor, we're stuck — caller should handle
}

QStringList extractNativesJar(const QString &jarPath, const QString &destDir)
{
    QStringList extracted;
    QFile file(jarPath);
    if (!file.open(QIODevice::ReadOnly)) return extracted;

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    QDir().mkpath(destDir);

    while (!stream.atEnd()) {
        quint32 signature = readU32(stream);

        // Local File Header signature: 0x04034b50
        if (signature != 0x04034b50) break;

        quint16 versionNeeded = readU16(stream);
        Q_UNUSED(versionNeeded)
        quint16 flags = readU16(stream);
        quint16 compression = readU16(stream);
        quint16 modTime = readU16(stream);
        Q_UNUSED(modTime)
        quint16 modDate = readU16(stream);
        Q_UNUSED(modDate)
        quint32 crc32 = readU32(stream);
        Q_UNUSED(crc32)
        quint32 compressedSize = readU32(stream);
        quint32 uncompressedSize = readU32(stream);
        quint16 nameLen = readU16(stream);
        quint16 extraLen = readU16(stream);

        // Read filename
        QByteArray nameBytes(nameLen, Qt::Uninitialized);
        stream.readRawData(nameBytes.data(), nameLen);
        QString fileName = QString::fromUtf8(nameBytes);

        // Skip extra field
        if (extraLen > 0) {
            QByteArray extra(extraLen, Qt::Uninitialized);
            stream.readRawData(extra.data(), extraLen);
        }

        bool hasDataDescriptor = (flags & 0x08) != 0;

        if (!isNativeFile(fileName)) {
            skipCompressedData(file, stream, compressedSize, hasDataDescriptor);
            continue;
        }

        // Only support STORED (0) and DEFLATE (8)
        if (compression != 0 && compression != 8) {
            skipCompressedData(file, stream, compressedSize, hasDataDescriptor);
            continue;
        }

        // Read compressed data
        if (compressedSize == 0 && hasDataDescriptor) {
            // Can't extract — size unknown. Skip.
            skipCompressedData(file, stream, compressedSize, hasDataDescriptor);
            continue;
        }

        QByteArray data(compressedSize, Qt::Uninitialized);
        stream.readRawData(data.data(), compressedSize);

        QString outPath = destDir + "/" + QFileInfo(fileName).fileName();
        QFile outFile(outPath);
        if (outFile.open(QIODevice::WriteOnly)) {
            if (compression == 0) {
                // STORED
                outFile.write(data);
            } else {
                // DEFLATE — proper zlib raw inflate
                QByteArray decompressed = inflateRaw(data, uncompressedSize);
                if (decompressed.isEmpty() && uncompressedSize > 0) {
                    // Decompression failed — skip this file
                    outFile.close();
                    QFile::remove(outPath);
                    if (hasDataDescriptor) {
                        quint32 descSig = readU32(stream);
                        if (descSig == 0x08074b50) {
                            readU32(stream); readU32(stream); readU32(stream);
                        }
                    }
                    continue;
                }
                outFile.write(decompressed);
            }
            outFile.close();
            extracted.append(outPath);
        }

        // Consume data descriptor if present
        if (hasDataDescriptor) {
            quint32 descSig = readU32(stream);
            if (descSig == 0x08074b50) {
                readU32(stream); // CRC
                readU32(stream); // compressed size
                readU32(stream); // uncompressed size
            }
        }
    }

    file.close();
    return extracted;
}

} // namespace FileUtils

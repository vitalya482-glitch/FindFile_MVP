#include "index/BinaryIndexWriter.h"

#include "util/TimeUtils.h"
#include "util/UtfUtils.h"

#include <array>
#include <stdexcept>

namespace findfile::index
{
    namespace
    {
#pragma pack(push, 1)
        // FileHeader — заголовок всего .ffdb файла.
        struct FileHeader
        {
            char magic[8] = { 'F','F','D','B','0','0','0','1' };
            std::uint32_t version = 1;
            std::uint64_t createdAt = 0;
        };

        // BlockHeader — заголовок одного commit-блока.
        struct BlockHeader
        {
            char magic[4] = { 'B','L','K','1' };
            std::uint64_t commitId = 0;
            std::uint64_t stringCount = 0;
            std::uint64_t folderCount = 0;
            std::uint64_t fileCount = 0;
        };

        // BlockEnd — маркер завершённого блока.
        // Если он есть, значит блок можно читать поиском даже во время индексации.
        struct BlockEnd
        {
            char magic[4] = { 'E','N','D','1' };
            std::uint64_t commitId = 0;
            std::uint64_t reservedChecksum = 0;
        };
#pragma pack(pop)
    }

    BinaryIndexWriter::BinaryIndexWriter(std::filesystem::path outputPath)
        : outputPath_(std::move(outputPath))
    {
    }

    BinaryIndexWriter::~BinaryIndexWriter()
    {
        if (stream_.is_open())
        {
            stream_.flush();
            stream_.close();
        }
    }

    void BinaryIndexWriter::openNew()
    {
        stream_.open(outputPath_, std::ios::binary | std::ios::trunc);
        if (!stream_.is_open())
        {
            throw std::runtime_error("Cannot open output index file");
        }

        FileHeader header;
        header.createdAt = util::nowUnixSeconds();
        writePod(header);
        stream_.flush();
    }

    void BinaryIndexWriter::writeBatch(const IndexBatch& batch, std::uint64_t commitId)
    {
        if (!stream_.is_open())
        {
            throw std::runtime_error("Index writer is not open");
        }

        BlockHeader block;
        block.commitId = commitId;
        block.stringCount = batch.strings.size();
        block.folderCount = batch.folders.size();
        block.fileCount = batch.files.size();
        writePod(block);

        for (const auto& record : batch.strings)
        {
            writeUtf8String(record.id, record.value);
        }

        for (const auto& folder : batch.folders)
        {
            writePod(folder);
        }

        for (const auto& file : batch.files)
        {
            writePod(file);
        }

        BlockEnd end;
        end.commitId = commitId;
        writePod(end);

        // flush после commit marker нужен, чтобы GUI мог читать уже завершённые блоки.
        stream_.flush();
    }

    void BinaryIndexWriter::finalize()
    {
        if (stream_.is_open())
        {
            stream_.flush();
        }
    }

    void BinaryIndexWriter::writeUtf8String(std::uint32_t id, const std::wstring& value)
    {
        const std::string utf8 = util::wideToUtf8(value);
        writePod(id);
        const auto length = static_cast<std::uint32_t>(utf8.size());
        writePod(length);
        if (length > 0)
        {
            stream_.write(utf8.data(), length);
        }
    }
}

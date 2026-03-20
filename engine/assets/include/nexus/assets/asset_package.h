#pragma once

#include "nexus/assets/asset_handle.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>

namespace nexus::assets {

// ─────────────────────────────────────────────────────────────────────────────
// AssetPackage — PAK file for bundling assets for distribution
// ─────────────────────────────────────────────────────────────────────────────

/// PAK file format:
///   [Header]         - magic, version, entry count, TOC offset
///   [Data blocks...] - raw asset data concatenated
///   [TOC entries...] - table of contents at end of file
///
/// The TOC is at the end so we can build the file by appending data,
/// then writing the TOC after all entries.

struct PackageHeader {
    u32 magic{0x4E585041};   // "NXPA" (NexusPackage)
    u32 version{1};
    u32 entry_count{0};
    u64 toc_offset{0};      // Byte offset to start of TOC
};

struct PackageEntry {
    AssetId id;
    AssetType type{AssetType::Unknown};
    std::string path;         // Virtual path
    u64 offset{0};            // Byte offset in file
    u64 size{0};              // Uncompressed size
    u64 compressed_size{0};   // 0 = not compressed
    u32 checksum{0};          // CRC32

    bool is_compressed() const { return compressed_size > 0 && compressed_size < size; }
};

class AssetPackage {
public:
    AssetPackage() = default;

    /// Create a new package for writing.
    bool create(const std::string& output_path);

    /// Open an existing package for reading.
    bool open(const std::string& path);

    /// Close the package.
    void close();

    /// Is the package open?
    bool is_open() const { return is_open_; }

    /// Add a file to the package (during creation).
    bool add_file(const std::string& virtual_path, const std::string& source_path,
                  AssetType type = AssetType::Unknown);

    /// Add raw data to the package (during creation).
    bool add_data(const std::string& virtual_path, const std::vector<u8>& data,
                  AssetType type = AssetType::Unknown);

    /// Finalize the package (writes TOC and header). Must call after adding files.
    bool finalize();

    /// Read an entry's data from the package.
    std::vector<u8> read_entry(const std::string& virtual_path) const;
    std::vector<u8> read_entry(AssetId id) const;

    /// Check if an entry exists.
    bool has_entry(const std::string& virtual_path) const;
    bool has_entry(AssetId id) const;

    /// Get entry metadata.
    const PackageEntry* find_entry(const std::string& virtual_path) const;
    const PackageEntry* find_entry(AssetId id) const;

    /// Get all entries.
    const std::vector<PackageEntry>& entries() const { return entries_; }

    /// Number of entries.
    u32 entry_count() const { return static_cast<u32>(entries_.size()); }

    /// Total data size across all entries.
    u64 total_data_size() const;

    /// File path of the package.
    const std::string& file_path() const { return file_path_; }

    /// Validate package integrity (check header magic and checksums).
    bool validate() const;

    /// Compute CRC32 of data.
    static u32 crc32(const u8* data, u64 size);

private:
    bool write_header();
    bool write_toc();
    bool read_toc();
    std::vector<u8> read_entry_impl(const PackageEntry& entry) const;

    std::string file_path_;
    mutable std::fstream stream_;
    std::vector<PackageEntry> entries_;
    std::unordered_map<AssetId, u32> id_lookup_;    // id → entry index
    std::unordered_map<std::string, u32> path_lookup_; // path → entry index
    bool is_open_{false};
    bool is_writing_{false};
    u64 data_offset_{sizeof(PackageHeader)}; // current write position
};

// ─────────────────────────────────────────────────────────────────────────────
// AssetHotReload — watches for file changes and triggers reimport
// ─────────────────────────────────────────────────────────────────────────────

class AssetHotReload {
public:
    AssetHotReload() = default;

    /// Add a directory to watch.
    void watch(const std::string& directory);

    /// Remove a watch.
    void unwatch(const std::string& directory);

    /// Check for changes. Returns paths of modified files.
    std::vector<std::string> poll_changes();

    /// Number of watched directories.
    u32 watch_count() const { return static_cast<u32>(watches_.size()); }

    /// Number of tracked files.
    u32 tracked_file_count() const;

private:
    struct WatchEntry {
        std::string directory;
        std::unordered_map<std::string, u64> file_times; // path → last modified
    };

    std::vector<WatchEntry> watches_;
};

} // namespace nexus::assets

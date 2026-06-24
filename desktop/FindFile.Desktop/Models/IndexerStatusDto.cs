using System.Text.Json.Serialization;

namespace FindFile.Desktop.Models;

/// <summary>
/// DTO для JSON-статуса, который печатает C++ FindFileIndexer.exe.
/// Нужен отдельно от IndexerStatus, потому что C++ отдаёт snake_case поля
/// и словарь extensions, а WPF-интерфейсу удобна модель с PascalCase и ObservableCollection.
/// </summary>
public sealed class IndexerStatusDto
{
    [JsonPropertyName("status")]
    public string? Status { get; set; }

    [JsonPropertyName("root")]
    public string? Root { get; set; }

    [JsonPropertyName("index_path")]
    public string? IndexPath { get; set; }

    [JsonPropertyName("mode")]
    public string? Mode { get; set; }

    [JsonPropertyName("current_path")]
    public string? CurrentPath { get; set; }

    [JsonPropertyName("files_indexed")]
    public long FilesIndexed { get; set; }

    [JsonPropertyName("folders_indexed")]
    public long FoldersIndexed { get; set; }

    [JsonPropertyName("objects_indexed")]
    public long ObjectsIndexed { get; set; }

    [JsonPropertyName("errors")]
    public long Errors { get; set; }

    [JsonPropertyName("total_size")]
    public long TotalSize { get; set; }

    [JsonPropertyName("index_size")]
    public long IndexSize { get; set; }

    [JsonPropertyName("elapsed_sec")]
    public long ElapsedSeconds { get; set; }

    [JsonPropertyName("committed_files")]
    public long CommittedFiles { get; set; }

    [JsonPropertyName("committed_folders")]
    public long CommittedFolders { get; set; }

    [JsonPropertyName("commit_id")]
    public long CommitId { get; set; }

    [JsonPropertyName("extensions")]
    public Dictionary<string, long> Extensions { get; set; } = new(StringComparer.OrdinalIgnoreCase);
}

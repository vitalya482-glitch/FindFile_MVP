namespace FindFile.Desktop.Models;

public sealed class UpdateCheckResult
{
    public bool IsUpdateAvailable { get; init; }
    public bool IsMandatory { get; init; }
    public string CurrentVersion { get; init; } = string.Empty;
    public string LatestVersion { get; init; } = string.Empty;
    public string ReleaseDate { get; init; } = string.Empty;
    public string PackageUrl { get; init; } = string.Empty;
    public string PackageSha256 { get; init; } = string.Empty;
    public long PackageSize { get; init; }
    public IReadOnlyList<string> Notes { get; init; } = Array.Empty<string>();
    public UpdateConfig Config { get; init; } = new();
    public UpdateManifest Manifest { get; init; } = new();
}

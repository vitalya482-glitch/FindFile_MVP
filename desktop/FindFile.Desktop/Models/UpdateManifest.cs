namespace FindFile.Desktop.Models;

/// <summary>
/// Публичный manifest обновления из LVK-Update-Feed.
/// </summary>
public sealed class UpdateManifest
{
    public int SchemaVersion { get; set; }
    public string AppId { get; set; } = string.Empty;
    public string Name { get; set; } = string.Empty;
    public string LatestVersion { get; set; } = string.Empty;
    public string Channel { get; set; } = "stable";
    public bool Mandatory { get; set; }
    public string ReleaseDate { get; set; } = string.Empty;
    public string MainExe { get; set; } = "FindFile.exe";
    public UpdatePackage Package { get; set; } = new();
    public List<string> Notes { get; set; } = new();
}

public sealed class UpdatePackage
{
    public string Url { get; set; } = string.Empty;
    public string Sha256 { get; set; } = string.Empty;
    public long Size { get; set; }
}

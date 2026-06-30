namespace FindFile.Desktop.Models;

/// <summary>
/// Локальная конфигурация обновлений. Файл создаётся GitHub Actions и кладётся рядом с FindFile.exe.
/// </summary>
public sealed class UpdateConfig
{
    public int SchemaVersion { get; set; } = 1;
    public string AppId { get; set; } = "findfile";
    public string Name { get; set; } = "FindFile";
    public string CurrentVersion { get; set; } = "0.0.0";
    public string Channel { get; set; } = "stable";
    public string MainExe { get; set; } = "FindFile.exe";
    public string ManifestUrl { get; set; } = "https://raw.githubusercontent.com/vitalya482-glitch/LVK-Update-Feed/main/manifests/findfile.json";
}

using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text.Json;
using FindFile.Desktop.Models;

namespace FindFile.Desktop.Services;

/// <summary>
/// Клиент обновлений FindFile. Он только проверяет manifest, скачивает zip и запускает LVKUpdater.exe.
/// Сама замена файлов выполняется отдельным C++ updater-ом.
/// </summary>
public sealed class UpdateService
{
    private const string LocalUpdateConfigFileName = "app.update.json";
    private const string UpdaterExeFileName = "LVKUpdater.exe";
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web)
    {
        PropertyNameCaseInsensitive = true,
        WriteIndented = true
    };

    private readonly HttpClient _httpClient = new();

    public async Task<UpdateCheckResult> CheckForUpdatesAsync(CancellationToken cancellationToken = default)
    {
        var config = await LoadConfigAsync(cancellationToken);

        if (string.IsNullOrWhiteSpace(config.ManifestUrl))
        {
            throw new InvalidOperationException("В app.update.json не указан manifestUrl.");
        }

        using var response = await _httpClient.GetAsync(config.ManifestUrl, cancellationToken);
        response.EnsureSuccessStatusCode();

        await using var stream = await response.Content.ReadAsStreamAsync(cancellationToken);
        var manifest = await JsonSerializer.DeserializeAsync<UpdateManifest>(stream, JsonOptions, cancellationToken)
                       ?? throw new InvalidOperationException("Не удалось прочитать manifest обновления.");

        if (!string.Equals(config.AppId, manifest.AppId, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException($"Manifest предназначен для другого приложения: {manifest.AppId}.");
        }

        var isUpdateAvailable = CompareVersions(manifest.LatestVersion, config.CurrentVersion) > 0;

        return new UpdateCheckResult
        {
            IsUpdateAvailable = isUpdateAvailable,
            IsMandatory = manifest.Mandatory,
            CurrentVersion = config.CurrentVersion,
            LatestVersion = manifest.LatestVersion,
            ReleaseDate = manifest.ReleaseDate,
            PackageUrl = manifest.Package.Url,
            PackageSha256 = manifest.Package.Sha256,
            PackageSize = manifest.Package.Size,
            Notes = manifest.Notes,
            Config = config,
            Manifest = manifest
        };
    }

    public async Task<string> DownloadPackageAsync(UpdateCheckResult update, IProgress<double>? progress = null, CancellationToken cancellationToken = default)
    {
        if (string.IsNullOrWhiteSpace(update.PackageUrl))
        {
            throw new InvalidOperationException("В manifest нет ссылки на пакет обновления.");
        }

        var tempDir = Path.Combine(Path.GetTempPath(), "FindFile", "updates");
        Directory.CreateDirectory(tempDir);

        var packagePath = Path.Combine(tempDir, $"FindFile-{update.LatestVersion}-win-x64.zip");
        if (File.Exists(packagePath))
        {
            File.Delete(packagePath);
        }

        using var response = await _httpClient.GetAsync(update.PackageUrl, HttpCompletionOption.ResponseHeadersRead, cancellationToken);
        response.EnsureSuccessStatusCode();

        var totalBytes = response.Content.Headers.ContentLength ?? update.PackageSize;
        await using var input = await response.Content.ReadAsStreamAsync(cancellationToken);
        await using var output = File.Create(packagePath);

        var buffer = new byte[1024 * 128];
        long readTotal = 0;

        while (true)
        {
            var read = await input.ReadAsync(buffer, cancellationToken);
            if (read == 0)
            {
                break;
            }

            await output.WriteAsync(buffer.AsMemory(0, read), cancellationToken);
            readTotal += read;

            if (totalBytes > 0)
            {
                progress?.Report(readTotal * 100.0 / totalBytes);
            }
        }

        progress?.Report(100);

        if (!string.IsNullOrWhiteSpace(update.PackageSha256))
        {
            var actualHash = await ComputeSha256Async(packagePath, cancellationToken);
            if (!string.Equals(actualHash, update.PackageSha256, StringComparison.OrdinalIgnoreCase))
            {
                File.Delete(packagePath);
                throw new InvalidOperationException($"SHA256 не совпал. Ожидали {update.PackageSha256}, получили {actualHash}.");
            }
        }

        return packagePath;
    }

    public void StartUpdater(UpdateCheckResult update, string packagePath)
    {
        var appDir = AppContext.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        var updaterPath = Path.Combine(appDir, UpdaterExeFileName);

        if (!File.Exists(updaterPath))
        {
            throw new FileNotFoundException("LVKUpdater.exe не найден рядом с FindFile.exe.", updaterPath);
        }

        if (!File.Exists(packagePath))
        {
            throw new FileNotFoundException("Пакет обновления не найден.", packagePath);
        }

        var currentProcess = Process.GetCurrentProcess();
        var mainExe = string.IsNullOrWhiteSpace(update.Config.MainExe) ? "FindFile.exe" : update.Config.MainExe;

        var arguments = string.Join(" ", new[]
        {
            "--app-id", Quote(update.Config.AppId),
            "--app-dir", Quote(appDir),
            "--package", Quote(packagePath),
            "--main-exe", Quote(mainExe),
            "--wait-pid", currentProcess.Id.ToString()
        });

        var startInfo = new ProcessStartInfo
        {
            FileName = updaterPath,
            Arguments = arguments,
            WorkingDirectory = appDir,
            UseShellExecute = true
        };

        Process.Start(startInfo);
    }

    private static async Task<UpdateConfig> LoadConfigAsync(CancellationToken cancellationToken)
    {
        var configPath = Path.Combine(AppContext.BaseDirectory, LocalUpdateConfigFileName);

        if (!File.Exists(configPath))
        {
            return new UpdateConfig
            {
                CurrentVersion = "0.0.0"
            };
        }

        await using var stream = File.OpenRead(configPath);
        return await JsonSerializer.DeserializeAsync<UpdateConfig>(stream, JsonOptions, cancellationToken)
               ?? new UpdateConfig { CurrentVersion = "0.0.0" };
    }

    private static async Task<string> ComputeSha256Async(string filePath, CancellationToken cancellationToken)
    {
        await using var stream = File.OpenRead(filePath);
        var hash = await SHA256.HashDataAsync(stream, cancellationToken);
        return Convert.ToHexString(hash).ToLowerInvariant();
    }

    private static int CompareVersions(string left, string right)
    {
        var normalizedLeft = NormalizeVersion(left);
        var normalizedRight = NormalizeVersion(right);

        if (Version.TryParse(normalizedLeft, out var leftVersion) && Version.TryParse(normalizedRight, out var rightVersion))
        {
            return leftVersion.CompareTo(rightVersion);
        }

        return string.Compare(normalizedLeft, normalizedRight, StringComparison.OrdinalIgnoreCase);
    }

    private static string NormalizeVersion(string version)
    {
        version = (version ?? string.Empty).Trim();
        if (version.StartsWith("v", StringComparison.OrdinalIgnoreCase))
        {
            version = version[1..];
        }

        return string.IsNullOrWhiteSpace(version) ? "0.0.0" : version;
    }

    private static string Quote(string value) => $"\"{value.Replace("\"", "\\\"")}\"";
}

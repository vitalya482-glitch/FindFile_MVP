using System.Diagnostics;
using System.IO;
using System.Text;
using System.Text.Json;
using FindFile.Desktop.Models;

namespace FindFile.Desktop.Services;

/// <summary>
/// Сервис связи WPF GUI с C++ индексатором.
/// Он не индексирует файлы сам. Его задача — найти FindFileIndexer.exe, запустить команды и вернуть результат ViewModel.
/// </summary>
public sealed class IndexerProcessService
{
    private readonly string _indexerExePath;

    public IndexerProcessService()
    {
        // Ищем FindFileIndexer.exe рядом с FindFile.exe.
        _indexerExePath = Path.Combine(AppContext.BaseDirectory, "FindFileIndexer.exe");
    }

    /// <summary>
    /// Проверяет, существует ли C++ индексатор рядом с GUI.
    /// Это позволит показать понятную ошибку, если exe ещё не положили в релиз.
    /// </summary>
    public bool IsIndexerAvailable() => File.Exists(_indexerExePath);

    /// <summary>
    /// Запускает долгую индексацию.
    /// Команда index стартует отдельный процесс C++ индексатора.
    /// </summary>
    public void StartIndexing(IndexerSettings settings, IEnumerable<ExtensionOption> extensionOptions)
    {
        EnsureIndexerExists();

        Directory.CreateDirectory(Path.GetDirectoryName(settings.IndexPath) ?? AppContext.BaseDirectory);

        string includeExt = BuildIncludeExtensions(extensionOptions, settings.CustomExtensions);
        string fields = BuildFields(settings);

        var args = new StringBuilder();
        args.Append("index ");
        args.Append($"--root {Quote(settings.RootPath)} ");
        args.Append($"--output {Quote(settings.IndexPath)} ");
        args.Append("--session main ");
        args.Append("--batch 5000 ");
        args.Append($"--mode {settings.IndexMode} ");
        args.Append($"--include-ext {Quote(includeExt)} ");
        args.Append($"--fields {Quote(fields)} ");
        args.Append($"--exclude-dir {Quote(settings.ExcludeDirs)} ");
        args.Append($"--exclude-file {Quote(settings.ExcludeMasks)} ");

        StartDetached(args.ToString());
    }

    /// <summary>Передаёт C++ индексатору команду pause через короткий запуск exe.</summary>
    public void Pause() => RunShortCommand("pause --session main");

    /// <summary>Передаёт C++ индексатору команду resume через короткий запуск exe.</summary>
    public void Resume() => RunShortCommand("resume --session main");

    /// <summary>Передаёт C++ индексатору команду stop через короткий запуск exe.</summary>
    public void Stop() => RunShortCommand("stop --session main");

    /// <summary>
    /// Запрашивает статус у индексатора.
    /// Позже этот метод можно заменить на чтение shared memory напрямую.
    /// </summary>
    public IndexerStatus GetStatus()
    {
        if (!IsIndexerAvailable())
        {
            return new IndexerStatus
            {
                State = "Индексатор не найден",
                CurrentPath = _indexerExePath
            };
        }

        try
        {
            string json = RunAndCapture("status --session main");
            var status = JsonSerializer.Deserialize<IndexerStatus>(json, new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            });

            return status ?? new IndexerStatus { State = "Нет статуса" };
        }
        catch
        {
            return new IndexerStatus { State = "Статус недоступен" };
        }
    }

    private void EnsureIndexerExists()
    {
        if (!IsIndexerAvailable())
        {
            throw new FileNotFoundException("FindFileIndexer.exe не найден рядом с FindFile.exe", _indexerExePath);
        }
    }

    private void StartDetached(string arguments)
    {
        var psi = new ProcessStartInfo
        {
            FileName = _indexerExePath,
            Arguments = arguments,
            UseShellExecute = false,
            CreateNoWindow = true,
            WorkingDirectory = AppContext.BaseDirectory
        };
        Process.Start(psi);
    }

    private void RunShortCommand(string arguments)
    {
        EnsureIndexerExists();
        using var process = Process.Start(new ProcessStartInfo
        {
            FileName = _indexerExePath,
            Arguments = arguments,
            UseShellExecute = false,
            CreateNoWindow = true,
            WorkingDirectory = AppContext.BaseDirectory
        });
        process?.WaitForExit(3000);
    }

    private string RunAndCapture(string arguments)
    {
        using var process = Process.Start(new ProcessStartInfo
        {
            FileName = _indexerExePath,
            Arguments = arguments,
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            WorkingDirectory = AppContext.BaseDirectory
        });

        if (process == null)
        {
            return string.Empty;
        }

        string output = process.StandardOutput.ReadToEnd();
        process.WaitForExit(3000);
        return output;
    }

    private static string Quote(string value) => $"\"{value}\"";

    private static string BuildIncludeExtensions(IEnumerable<ExtensionOption> options, string customExtensions)
    {
        var selected = options
            .Where(o => o.IsEnabled)
            .SelectMany(o => o.Extensions.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries));

        var custom = customExtensions.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
        return string.Join(',', selected.Concat(custom).Distinct(StringComparer.OrdinalIgnoreCase));
    }

    private static string BuildFields(IndexerSettings settings)
    {
        var fields = new List<string>();
        if (settings.StoreName) fields.Add("name");
        if (settings.StorePath) fields.Add("path");
        if (settings.StoreSize) fields.Add("size");
        if (settings.StoreModifiedTime) fields.Add("mtime");
        if (settings.StoreExtension) fields.Add("ext");
        if (settings.StoreAttributes) fields.Add("attrs");
        if (settings.StoreFileType) fields.Add("type");
        return string.Join(',', fields);
    }
}

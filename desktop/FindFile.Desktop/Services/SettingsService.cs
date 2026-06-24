using System.IO;
using System.Text.Json;
using FindFile.Desktop.Models;

namespace FindFile.Desktop.Services;

/// <summary>
/// Сервис настроек приложения.
/// Отвечает только за чтение и запись settings.json рядом с FindFile.exe.
/// ViewModel обращается сюда, когда нужно загрузить или сохранить настройки индексатора.
/// </summary>
public sealed class SettingsService
{
    private readonly string _settingsPath;

    public SettingsService()
    {
        _settingsPath = Path.Combine(AppContext.BaseDirectory, "settings.json");
    }

    /// <summary>
    /// Загружает настройки из JSON. Если файла нет или он повреждён — возвращает настройки по умолчанию.
    /// </summary>
    public IndexerSettings LoadIndexerSettings()
    {
        try
        {
            if (!File.Exists(_settingsPath))
            {
                return new IndexerSettings();
            }

            string json = File.ReadAllText(_settingsPath);
            return JsonSerializer.Deserialize<IndexerSettings>(json) ?? new IndexerSettings();
        }
        catch
        {
            return new IndexerSettings();
        }
    }

    /// <summary>
    /// Сохраняет настройки в settings.json рядом с программой.
    /// </summary>
    public void SaveIndexerSettings(IndexerSettings settings)
    {
        var options = new JsonSerializerOptions
        {
            WriteIndented = true
        };

        string json = JsonSerializer.Serialize(settings, options);
        File.WriteAllText(_settingsPath, json);
    }
}

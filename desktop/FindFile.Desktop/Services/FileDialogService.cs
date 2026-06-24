using Microsoft.Win32;

namespace FindFile.Desktop.Services;

/// <summary>
/// Сервис диалоговых окон выбора папки и файла.
/// Нужен, чтобы ViewModel не знала напрямую про WPF/Windows dialogs.
/// </summary>
public sealed class FileDialogService
{
    /// <summary>
    /// Открывает окно выбора папки индексации.
    /// В .NET 8 WPF доступен Microsoft.Win32.OpenFolderDialog.
    /// </summary>
    public string? SelectFolder(string? initialPath)
    {
        var dialog = new OpenFolderDialog
        {
            Title = "Выберите папку для индексации",
            Multiselect = false
        };

        if (!string.IsNullOrWhiteSpace(initialPath))
        {
            dialog.InitialDirectory = initialPath;
        }

        return dialog.ShowDialog() == true ? dialog.FolderName : null;
    }

    /// <summary>
    /// Открывает окно выбора пути сохранения индекса .ffdb.
    /// </summary>
    public string? SelectIndexFile(string? initialPath)
    {
        var dialog = new SaveFileDialog
        {
            Title = "Выберите путь сохранения индекса",
            Filter = "FindFile index (*.ffdb)|*.ffdb|All files (*.*)|*.*",
            FileName = string.IsNullOrWhiteSpace(initialPath) ? "file_index.ffdb" : System.IO.Path.GetFileName(initialPath)
        };

        string? dir = string.IsNullOrWhiteSpace(initialPath) ? null : System.IO.Path.GetDirectoryName(initialPath);
        if (!string.IsNullOrWhiteSpace(dir))
        {
            dialog.InitialDirectory = dir;
        }

        return dialog.ShowDialog() == true ? dialog.FileName : null;
    }
}

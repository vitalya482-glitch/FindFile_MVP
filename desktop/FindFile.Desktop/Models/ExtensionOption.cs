using FindFile.Desktop.Core;

namespace FindFile.Desktop.Models;

/// <summary>
/// Один флажок типа файлов на странице индексатора.
/// Например: PDF, Word, Excel, DWG.
/// ViewModel хранит коллекцию таких объектов, а XAML рисует их как CheckBox.
/// </summary>
public sealed class ExtensionOption : ObservableObject
{
    private bool _isEnabled;

    public ExtensionOption(string title, string extensions, bool isEnabled)
    {
        Title = title;
        Extensions = extensions;
        _isEnabled = isEnabled;
    }

    /// <summary>Название группы в интерфейсе: PDF, Word, Изображения.</summary>
    public string Title { get; }

    /// <summary>Фактические расширения, которые будут переданы индексатору.</summary>
    public string Extensions { get; }

    /// <summary>Выбран ли флажок пользователем.</summary>
    public bool IsEnabled
    {
        get => _isEnabled;
        set => SetProperty(ref _isEnabled, value);
    }
}

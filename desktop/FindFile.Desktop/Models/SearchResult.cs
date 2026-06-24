namespace FindFile.Desktop.Models;

/// <summary>
/// Один результат поиска.
/// Сейчас это модель-заготовка. Позже SearchViewModel будет получать такие записи из C++ индексатора.
/// </summary>
public sealed class SearchResult
{
    public string Name { get; set; } = string.Empty;
    public string Path { get; set; } = string.Empty;
    public string Extension { get; set; } = string.Empty;
    public long Size { get; set; }
    public DateTime ModifiedTime { get; set; }
}

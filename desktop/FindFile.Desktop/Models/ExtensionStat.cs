namespace FindFile.Desktop.Models;

/// <summary>
/// Статистика по одному расширению.
/// Эти данные приходят из статуса C++ индексатора и показываются справа на странице.
/// </summary>
public sealed class ExtensionStat
{
    public string Extension { get; set; } = string.Empty;
    public long Count { get; set; }
}

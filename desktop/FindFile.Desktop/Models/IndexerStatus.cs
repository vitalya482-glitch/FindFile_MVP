using System.Collections.ObjectModel;
using FindFile.Desktop.Core;

namespace FindFile.Desktop.Models;

/// <summary>
/// Живой статус индексатора.
/// В будущем будет заполняться из shared memory C++ индексатора.
/// В первой версии ViewModel может обновлять его через команду status или временно имитировать.
/// </summary>
public sealed class IndexerStatus : ObservableObject
{
    private string _state = "Ожидание";
    private string _currentPath = "—";
    private long _filesIndexed;
    private long _foldersIndexed;
    private long _errors;
    private long _indexSizeBytes;
    private double _progressPercent;
    private string _elapsed = "00:00:00";
    private string _remaining = "—";
    private string _speed = "—";

    public string State { get => _state; set => SetProperty(ref _state, value); }
    public string CurrentPath { get => _currentPath; set => SetProperty(ref _currentPath, value); }
    public long FilesIndexed { get => _filesIndexed; set => SetProperty(ref _filesIndexed, value); }
    public long FoldersIndexed { get => _foldersIndexed; set => SetProperty(ref _foldersIndexed, value); }
    public long Errors { get => _errors; set => SetProperty(ref _errors, value); }
    public long IndexSizeBytes { get => _indexSizeBytes; set => SetProperty(ref _indexSizeBytes, value); }
    public double ProgressPercent { get => _progressPercent; set => SetProperty(ref _progressPercent, value); }
    public string Elapsed { get => _elapsed; set => SetProperty(ref _elapsed, value); }
    public string Remaining { get => _remaining; set => SetProperty(ref _remaining, value); }
    public string Speed { get => _speed; set => SetProperty(ref _speed, value); }

    /// <summary>Список статистики расширений для правой панели.</summary>
    public ObservableCollection<ExtensionStat> ExtensionStats { get; } = new();

    public string IndexSizeText => IndexSizeBytes <= 0 ? "—" : $"{IndexSizeBytes / 1024.0 / 1024.0:0.0} МБ";
}

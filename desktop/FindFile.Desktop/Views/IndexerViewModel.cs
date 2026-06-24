using System.Collections.ObjectModel;
using System.Windows;
using System.Windows.Input;
using System.Windows.Threading;
using FindFile.Desktop.Core;
using FindFile.Desktop.Models;
using FindFile.Desktop.Services;

namespace FindFile.Desktop.ViewModels;

/// <summary>
/// ViewModel страницы "Индексатор".
/// Она связывает XAML-интерфейс с настройками, кнопками и C++ индексатором.
/// </summary>
public sealed class IndexerViewModel : ObservableObject
{
    private readonly SettingsService _settingsService;
    private readonly FileDialogService _dialogService;
    private readonly IndexerProcessService _indexerService;
    private readonly DispatcherTimer _statusTimer;

    private IndexerSettings _settings;
    private IndexerStatus _status = new();

    public IndexerViewModel(SettingsService settingsService, FileDialogService dialogService, IndexerProcessService indexerService)
    {
        _settingsService = settingsService;
        _dialogService = dialogService;
        _indexerService = indexerService;
        _settings = _settingsService.LoadIndexerSettings();

        ExtensionOptions = new ObservableCollection<ExtensionOption>
        {
            new("PDF", ".pdf", true),
            new("Word (.doc/.docx)", ".doc,.docx", true),
            new("Excel/CSV", ".xls,.xlsx,.csv", true),
            new("Изображения", ".jpg,.jpeg,.png,.bmp,.gif,.webp", true),
            new("Видео", ".mp4,.avi,.mkv,.mov", true),
            new("Архивы", ".zip,.rar,.7z", true),
            new("DWG", ".dwg,.dxf", true),
            new("TXT", ".txt,.log", true),
            new("Другие", "", true),
        };

        IndexModes = new ObservableCollection<string>
        {
            "full",
            "update",
            "resume"
        };

        BrowseRootCommand = new RelayCommand(BrowseRoot);
        BrowseIndexPathCommand = new RelayCommand(BrowseIndexPath);
        SaveSettingsCommand = new RelayCommand(SaveSettings);
        ResetSettingsCommand = new RelayCommand(ResetSettings);
        StartCommand = new RelayCommand(StartIndexing);
        PauseCommand = new RelayCommand(() => SafeIndexerCall(_indexerService.Pause));
        ResumeCommand = new RelayCommand(() => SafeIndexerCall(_indexerService.Resume));
        StopCommand = new RelayCommand(() => SafeIndexerCall(_indexerService.Stop));

        _statusTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromSeconds(1)
        };
        _statusTimer.Tick += (_, _) => RefreshStatus();

        SeedDemoStats();
    }

    public IndexerSettings Settings
    {
        get => _settings;
        set => SetProperty(ref _settings, value);
    }

    public IndexerStatus Status
    {
        get => _status;
        set => SetProperty(ref _status, value);
    }

    public ObservableCollection<ExtensionOption> ExtensionOptions { get; }
    public ObservableCollection<string> IndexModes { get; }

    public ICommand BrowseRootCommand { get; }
    public ICommand BrowseIndexPathCommand { get; }
    public ICommand SaveSettingsCommand { get; }
    public ICommand ResetSettingsCommand { get; }
    public ICommand StartCommand { get; }
    public ICommand PauseCommand { get; }
    public ICommand ResumeCommand { get; }
    public ICommand StopCommand { get; }

    private void BrowseRoot()
    {
        string? selected = _dialogService.SelectFolder(Settings.RootPath);
        if (!string.IsNullOrWhiteSpace(selected))
        {
            Settings.RootPath = selected;
        }
    }

    private void BrowseIndexPath()
    {
        string? selected = _dialogService.SelectIndexFile(Settings.IndexPath);
        if (!string.IsNullOrWhiteSpace(selected))
        {
            Settings.IndexPath = selected;
        }
    }

    private void SaveSettings()
    {
        _settingsService.SaveIndexerSettings(Settings);
        MessageBox.Show("Настройки сохранены.", "FindFile", MessageBoxButton.OK, MessageBoxImage.Information);
    }

    private void ResetSettings()
    {
        Settings = new IndexerSettings();
    }

    private void StartIndexing()
    {
        try
        {
            _settingsService.SaveIndexerSettings(Settings);
            _indexerService.StartIndexing(Settings, ExtensionOptions);
            Status.State = "Запущено";
            _statusTimer.Start();
        }
        catch (Exception ex)
        {
            MessageBox.Show(ex.Message, "Ошибка запуска индексатора", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private void SafeIndexerCall(Action action)
    {
        try
        {
            action();
            RefreshStatus();
        }
        catch (Exception ex)
        {
            MessageBox.Show(ex.Message, "Ошибка команды индексатора", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private void RefreshStatus()
    {
        // Статус приходит из C++ через отдельный DTO и уже содержит реальную статистику расширений.
        // Демонстрационные значения после первого запроса не подставляем, чтобы не показывать фейковые цифры.
        Status = _indexerService.GetStatus();
    }

    private void SeedDemoStats()
    {
        Status.ExtensionStats.Clear();
        Status.ExtensionStats.Add(new ExtensionStat { Extension = "PDF", Count = 82310 });
        Status.ExtensionStats.Add(new ExtensionStat { Extension = "DOCX", Count = 22104 });
        Status.ExtensionStats.Add(new ExtensionStat { Extension = "XLSX", Count = 9481 });
        Status.ExtensionStats.Add(new ExtensionStat { Extension = "JPG", Count = 40220 });
        Status.ExtensionStats.Add(new ExtensionStat { Extension = "DWG", Count = 3112 });
    }
}

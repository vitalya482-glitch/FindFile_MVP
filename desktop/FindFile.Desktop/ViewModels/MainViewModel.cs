using System.Windows.Input;
using FindFile.Desktop.Core;
using FindFile.Desktop.Services;

namespace FindFile.Desktop.ViewModels;

/// <summary>
/// Главная ViewModel приложения.
/// Она хранит все страницы и переключает CurrentPage, который отображается в MainWindow через ContentControl.
/// </summary>
public sealed class MainViewModel : ObservableObject
{
    private object _currentPage;

    public MainViewModel()
    {
        var settingsService = new SettingsService();
        var dialogService = new FileDialogService();
        var indexerService = new IndexerProcessService();

        IndexerPage = new IndexerViewModel(settingsService, dialogService, indexerService);
        SearchPage = new SearchViewModel();
        ResultsPage = new ResultsViewModel();
        SettingsPage = new SettingsViewModel();
        AboutPage = new AboutViewModel();

        _currentPage = IndexerPage;

        ShowIndexerCommand = new RelayCommand(() => CurrentPage = IndexerPage);
        ShowSearchCommand = new RelayCommand(() => CurrentPage = SearchPage);
        ShowResultsCommand = new RelayCommand(() => CurrentPage = ResultsPage);
        ShowSettingsCommand = new RelayCommand(() => CurrentPage = SettingsPage);
        ShowAboutCommand = new RelayCommand(() => CurrentPage = AboutPage);
    }

    public IndexerViewModel IndexerPage { get; }
    public SearchViewModel SearchPage { get; }
    public ResultsViewModel ResultsPage { get; }
    public SettingsViewModel SettingsPage { get; }
    public AboutViewModel AboutPage { get; }

    /// <summary>Текущая открытая страница. XAML сам подставляет нужный UserControl через DataTemplate.</summary>
    public object CurrentPage
    {
        get => _currentPage;
        set => SetProperty(ref _currentPage, value);
    }

    public ICommand ShowIndexerCommand { get; }
    public ICommand ShowSearchCommand { get; }
    public ICommand ShowResultsCommand { get; }
    public ICommand ShowSettingsCommand { get; }
    public ICommand ShowAboutCommand { get; }
}

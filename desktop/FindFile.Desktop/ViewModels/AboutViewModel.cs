using System.Windows;
using System.Windows.Input;
using FindFile.Desktop.Core;
using FindFile.Desktop.Models;
using FindFile.Desktop.Services;

namespace FindFile.Desktop.ViewModels;

/// <summary>
/// Страница "О программе" и ручная проверка обновлений.
/// </summary>
public sealed class AboutViewModel : ObservableObject
{
    private readonly UpdateService _updateService = new();
    private string _currentVersion = "неизвестно";
    private string _latestVersion = "не проверялась";
    private string _updateStatus = "Нажмите «Проверить обновления», чтобы сравнить локальную версию с LVK-Update-Feed.";
    private bool _isBusy;

    public AboutViewModel()
    {
        CheckUpdatesCommand = new RelayCommand(() => _ = CheckUpdatesAsync(), () => !IsBusy);
    }

    public string CurrentVersion
    {
        get => _currentVersion;
        private set => SetProperty(ref _currentVersion, value);
    }

    public string LatestVersion
    {
        get => _latestVersion;
        private set => SetProperty(ref _latestVersion, value);
    }

    public string UpdateStatus
    {
        get => _updateStatus;
        private set => SetProperty(ref _updateStatus, value);
    }

    public bool IsBusy
    {
        get => _isBusy;
        private set
        {
            if (SetProperty(ref _isBusy, value))
            {
                CommandManager.InvalidateRequerySuggested();
            }
        }
    }

    public ICommand CheckUpdatesCommand { get; }

    private async Task CheckUpdatesAsync()
    {
        try
        {
            IsBusy = true;
            UpdateStatus = "Проверяю manifest обновления...";

            var update = await _updateService.CheckForUpdatesAsync();
            CurrentVersion = update.CurrentVersion;
            LatestVersion = update.LatestVersion;

            if (!update.IsUpdateAvailable)
            {
                UpdateStatus = $"Обновление не требуется. Установлена актуальная версия {update.CurrentVersion}.";
                MessageBox.Show(UpdateStatus, "FindFile", MessageBoxButton.OK, MessageBoxImage.Information);
                return;
            }

            var notes = update.Notes.Count > 0 ? string.Join(Environment.NewLine, update.Notes.Select(note => "• " + note)) : "Описание изменений не указано.";
            var message = $"Доступна новая версия FindFile {update.LatestVersion}." + Environment.NewLine + Environment.NewLine +
                          $"Текущая версия: {update.CurrentVersion}" + Environment.NewLine +
                          $"Дата релиза: {update.ReleaseDate}" + Environment.NewLine + Environment.NewLine +
                          "Что нового:" + Environment.NewLine + notes + Environment.NewLine + Environment.NewLine +
                          "Скачать и установить обновление сейчас?";

            var answer = MessageBox.Show(message, "Доступно обновление", MessageBoxButton.YesNo, MessageBoxImage.Question);
            if (answer != MessageBoxResult.Yes)
            {
                UpdateStatus = "Обновление найдено, установка отложена.";
                return;
            }

            await DownloadAndInstallAsync(update);
        }
        catch (Exception ex)
        {
            UpdateStatus = "Ошибка проверки обновлений: " + ex.Message;
            MessageBox.Show(UpdateStatus, "Ошибка обновления", MessageBoxButton.OK, MessageBoxImage.Error);
        }
        finally
        {
            IsBusy = false;
        }
    }

    private async Task DownloadAndInstallAsync(UpdateCheckResult update)
    {
        var progress = new Progress<double>(value =>
        {
            UpdateStatus = $"Скачиваю обновление {update.LatestVersion}: {value:0}%";
        });

        var packagePath = await _updateService.DownloadPackageAsync(update, progress);

        UpdateStatus = "Пакет скачан и проверен. Запускаю LVKUpdater...";

        _updateService.StartUpdater(update, packagePath);

        Application.Current.Shutdown();
    }
}

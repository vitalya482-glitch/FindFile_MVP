using FindFile.Desktop.Core;

namespace FindFile.Desktop.Models;

/// <summary>
/// Настройки индексатора.
/// Сохраняются в settings.json и используются для запуска FindFileIndexer.exe.
/// Класс наследует ObservableObject, чтобы XAML обновлялся при изменениях.
/// </summary>
public sealed class IndexerSettings : ObservableObject
{
    private string _rootPath = @"\\Diskstationnew\Exchange";
    private string _indexPath = @"C:\FindFile\file_index.ffdb";
    private string _customExtensions = ".vsdx, .dwg, .rtf";
    private string _excludeDirs = "$RECYCLE.BIN; System Volume Information; __pycache__";
    private string _excludeMasks = "Thumbs.db; desktop.ini; *.tmp";
    private string _indexMode = "full";
    private bool _skipSystemFiles = true;
    private bool _skipHiddenFiles = true;
    private bool _skipAlreadyIndexed = true;
    private bool _storeName = true;
    private bool _storePath = true;
    private bool _storeSize = true;
    private bool _storeModifiedTime = true;
    private bool _storeExtension = true;
    private bool _storeAttributes = true;
    private bool _storeFileType = true;
    private bool _delayedStart;
    private string _delayedStartTime = "23:00";

    public string RootPath { get => _rootPath; set => SetProperty(ref _rootPath, value); }
    public string IndexPath { get => _indexPath; set => SetProperty(ref _indexPath, value); }
    public string CustomExtensions { get => _customExtensions; set => SetProperty(ref _customExtensions, value); }
    public string ExcludeDirs { get => _excludeDirs; set => SetProperty(ref _excludeDirs, value); }
    public string ExcludeMasks { get => _excludeMasks; set => SetProperty(ref _excludeMasks, value); }
    public string IndexMode { get => _indexMode; set => SetProperty(ref _indexMode, value); }

    public bool SkipSystemFiles { get => _skipSystemFiles; set => SetProperty(ref _skipSystemFiles, value); }
    public bool SkipHiddenFiles { get => _skipHiddenFiles; set => SetProperty(ref _skipHiddenFiles, value); }
    public bool SkipAlreadyIndexed { get => _skipAlreadyIndexed; set => SetProperty(ref _skipAlreadyIndexed, value); }

    public bool StoreName { get => _storeName; set => SetProperty(ref _storeName, value); }
    public bool StorePath { get => _storePath; set => SetProperty(ref _storePath, value); }
    public bool StoreSize { get => _storeSize; set => SetProperty(ref _storeSize, value); }
    public bool StoreModifiedTime { get => _storeModifiedTime; set => SetProperty(ref _storeModifiedTime, value); }
    public bool StoreExtension { get => _storeExtension; set => SetProperty(ref _storeExtension, value); }
    public bool StoreAttributes { get => _storeAttributes; set => SetProperty(ref _storeAttributes, value); }
    public bool StoreFileType { get => _storeFileType; set => SetProperty(ref _storeFileType, value); }

    public bool DelayedStart { get => _delayedStart; set => SetProperty(ref _delayedStart, value); }
    public string DelayedStartTime { get => _delayedStartTime; set => SetProperty(ref _delayedStartTime, value); }
}

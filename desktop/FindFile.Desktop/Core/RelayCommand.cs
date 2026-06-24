using System.Windows.Input;

namespace FindFile.Desktop.Core;

/// <summary>
/// Простая команда для кнопок WPF.
/// XAML-кнопка вызывает ICommand, а команда уже запускает метод ViewModel.
/// Так мы не пишем обработчики Click в code-behind окна.
/// </summary>
public sealed class RelayCommand : ICommand
{
    private readonly Action<object?> _execute;
    private readonly Predicate<object?>? _canExecute;

    public RelayCommand(Action execute)
        : this(_ => execute(), null)
    {
    }

    public RelayCommand(Action execute, Func<bool> canExecute)
        : this(_ => execute(), _ => canExecute())
    {
    }

    public RelayCommand(Action<object?> execute, Predicate<object?>? canExecute = null)
    {
        _execute = execute;
        _canExecute = canExecute;
    }

    public bool CanExecute(object? parameter) => _canExecute?.Invoke(parameter) ?? true;

    public void Execute(object? parameter) => _execute(parameter);

    public event EventHandler? CanExecuteChanged
    {
        add => CommandManager.RequerySuggested += value;
        remove => CommandManager.RequerySuggested -= value;
    }
}

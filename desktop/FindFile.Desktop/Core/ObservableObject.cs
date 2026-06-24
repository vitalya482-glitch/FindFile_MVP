using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace FindFile.Desktop.Core;

/// <summary>
/// Базовый класс для ViewModel и моделей настроек.
/// Он сообщает XAML-интерфейсу, что значение свойства изменилось.
/// Благодаря этому Binding обновляет текстовые поля, флажки и статус без ручного обновления окна.
/// </summary>
public abstract class ObservableObject : INotifyPropertyChanged
{
    public event PropertyChangedEventHandler? PropertyChanged;

    /// <summary>
    /// Универсальный setter для свойств.
    /// Если значение изменилось — сохраняет новое значение и уведомляет XAML.
    /// </summary>
    protected bool SetProperty<T>(ref T field, T value, [CallerMemberName] string? propertyName = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value))
        {
            return false;
        }

        field = value;
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        return true;
    }

    /// <summary>
    /// Ручное уведомление XAML о том, что свойство изменилось.
    /// Используется для вычисляемых свойств.
    /// </summary>
    protected void OnPropertyChanged([CallerMemberName] string? propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
}

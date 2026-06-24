using System.Windows;
using FindFile.Desktop.ViewModels;

namespace FindFile.Desktop;

/// <summary>
/// Главное окно приложения.
/// Важно: окно не содержит бизнес-логики. Оно только создаёт MainViewModel и отдаёт её в DataContext.
/// </summary>
public partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();
        DataContext = new MainViewModel();
    }
}

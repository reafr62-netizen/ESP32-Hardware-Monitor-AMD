#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QSettings>
#include <QCheckBox>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>

#include <ADLX.h>
#include <IPerformanceMonitoring.h>
#include <ADLXHelper.h>
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void atualizarDados();
    void alterarInicializacao(bool ativado);
    void mostrarJanela();
    void sairAplicativo();
    // Aplica a configuração das 4 linhas do OLED
    void aplicarConfiguracaoDisplay();

    // Restaura a configuração padrão das 4 linhas
    void restaurarConfiguracaoDisplay();





private:
    Ui::MainWindow *ui;
    QTimer *timer;

    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;

    // Valores atuais do computador
    int cpuAtual = 0;
    int ramAtual = 0;
    int gpuAtual = 0;
    int fpsAtual = 0;
    int temperaturaAtual = 0;
    int animacaoIndex = 0;


    // Envia as 4 linhas configuradas para o ESP32
    void enviarDisplay();
    HANDLE serial;

    void iniciarMonitoramento();


protected:
    void changeEvent(QEvent *event) override;
};

#endif
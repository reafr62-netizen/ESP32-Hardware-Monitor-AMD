#define _WIN32_WINNT 0x0600

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QSettings>
#include <QCoreApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QEvent>
#include <QStyle>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QApplication>
#include <QStringList>
#include <QByteArray>
#include <QCheckBox>

#include <cstdio>

using namespace adlx;

#pragma comment(lib, "pdh.lib")

// ============================================================
// CONFIGURAÇÃO
// ============================================================

const char* SERIAL_PORT = "\\\\.\\COM3";
const int BAUD_RATE = 115200;

// ============================================================
// CPU - PDH
// ============================================================

PDH_HQUERY cpuQuery = nullptr;
PDH_HCOUNTER cpuCounter = nullptr;

bool iniciarCPU()
{
    PDH_STATUS status = PdhOpenQueryA(NULL, 0, &cpuQuery);
    if (status != ERROR_SUCCESS)
        return false;

    status = PdhAddCounterA(cpuQuery, "\\Processor(_Total)\\% Processor Time", 0, &cpuCounter);
    if (status != ERROR_SUCCESS)
    {
        PdhCloseQuery(cpuQuery);
        cpuQuery = nullptr;
        cpuCounter = nullptr;
        return false;
    }

    PdhCollectQueryData(cpuQuery);
    return true;
}

int getCPUUsage()
{
    if (cpuQuery == nullptr || cpuCounter == nullptr)
        return 0;

    if (PdhCollectQueryData(cpuQuery) != ERROR_SUCCESS)
        return 0;

    PDH_FMT_COUNTERVALUE valor = {};
    if (PdhGetFormattedCounterValue(cpuCounter, PDH_FMT_DOUBLE, NULL, &valor) != ERROR_SUCCESS)
        return 0;

    int uso = static_cast<int>(valor.doubleValue);
    if (uso < 0) uso = 0;
    if (uso > 100) uso = 100;

    return uso;
}

void finalizarCPU()
{
    if (cpuQuery != nullptr)
    {
        PdhCloseQuery(cpuQuery);
        cpuQuery = nullptr;
        cpuCounter = nullptr;
    }
}

// ============================================================
// RAM
// ============================================================

int getRAMUsage()
{
    MEMORYSTATUSEX memory = {};
    memory.dwLength = sizeof(memory);

    if (!GlobalMemoryStatusEx(&memory))
        return 0;

    return static_cast<int>(memory.dwMemoryLoad);
}

// ============================================================
// SERIAL
// ============================================================

HANDLE abrirSerial()
{
    HANDLE serial = CreateFileA(
        SERIAL_PORT,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
        );

    if (serial == INVALID_HANDLE_VALUE)
        return INVALID_HANDLE_VALUE;

    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(serial, &dcb))
    {
        CloseHandle(serial);
        return INVALID_HANDLE_VALUE;
    }

    dcb.BaudRate = BAUD_RATE;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;

    if (!SetCommState(serial, &dcb))
    {
        CloseHandle(serial);
        return INVALID_HANDLE_VALUE;
    }

    return serial;
}

// ============================================================
// ADLX
// ============================================================

ADLXHelper adlxHelper;
IADLXSystem* adlxSystem = nullptr;
IADLXGPUPtr minhaGPU;
IADLXPerformanceMonitoringServicesPtr performanceMonitoring;

bool iniciarADLX()
{
    if (adlxHelper.Initialize() != ADLX_OK)
        return false;

    adlxSystem = adlxHelper.GetSystemServices();
    return (adlxSystem != nullptr);
}

bool encontrarGPUAMD()
{
    if (adlxSystem == nullptr)
        return false;

    IADLXGPUListPtr gpuList;
    if (adlxSystem->GetGPUs(&gpuList) != ADLX_OK || !gpuList)
        return false;

    adlx_uint quantidade = gpuList->Size();
    if (quantidade == 0)
        return false;

    for (adlx_uint i = 0; i < quantidade; i++)
    {
        IADLXGPUPtr gpu;
        if (gpuList->At(i, &gpu) != ADLX_OK || !gpu)
            continue;

        const char* nomeGPU = nullptr;
        if (gpu->Name(&nomeGPU) != ADLX_OK || nomeGPU == nullptr)
            continue;

        minhaGPU = gpu;
        return true;
    }

    return false;
}

bool iniciarMetricasGPU()
{
    if (!minhaGPU || adlxSystem == nullptr)
        return false;

    if (adlxSystem->GetPerformanceMonitoringServices(&performanceMonitoring) != ADLX_OK || !performanceMonitoring)
        return false;

    return true;
}

void getGPUInfo(int& usoGPU, int& temperaturaGPU, int& fps)
{
    usoGPU = 0;
    temperaturaGPU = 0;
    fps = 0;

    if (!performanceMonitoring || !minhaGPU)
        return;

    IADLXGPUMetricsPtr gpuMetrics;
    if (performanceMonitoring->GetCurrentGPUMetrics(minhaGPU, &gpuMetrics) == ADLX_OK && gpuMetrics)
    {
        adlx_double temperatura = 0;
        adlx_double utilizacao = 0;

        if (gpuMetrics->GPUTemperature(&temperatura) == ADLX_OK)
            temperaturaGPU = static_cast<int>(temperatura);

        if (gpuMetrics->GPUUsage(&utilizacao) == ADLX_OK)
            usoGPU = static_cast<int>(utilizacao);
    }

    IADLXFPSPtr fpsMetrics;
    if (performanceMonitoring->GetCurrentFPS(&fpsMetrics) == ADLX_OK && fpsMetrics)
    {
        adlx_int valorFPS = 0;
        if (fpsMetrics->FPS(&valorFPS) == ADLX_OK)
            fps = static_cast<int>(valorFPS);
    }
}

void finalizarADLX()
{
    performanceMonitoring = nullptr;
    minhaGPU = nullptr;
    adlxSystem = nullptr;
}

// ============================================================
// INICIAR JUNTO COM WINDOWS
// ============================================================

void MainWindow::alterarInicializacao(bool ativado)
{
    QSettings registro("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    const QString nome = "ESP32 PC Monitor";

    if (ativado)
    {
        QString caminho = QCoreApplication::applicationFilePath();
        caminho.replace("/", "\\");
        registro.setValue(nome, "\"" + caminho + "\"");
    }
    else
    {
        registro.remove(nome);
    }
}

// ============================================================
// GERAR TEXTO DE UMA LINHA
// ============================================================

QString gerarTextoLinha(const QString &tipo, const QString &nome, int cpu, int ram, int gpu, int fps, int temperatura)
{
    if (tipo == "CPU") return nome + "  " + QString::number(cpu) + "%";
    if (tipo == "RAM") return nome + "  " + QString::number(ram) + "%";
    if (tipo == "GPU") return nome + "  " + QString::number(gpu) + "%";
    if (tipo == "FPS")
    {
        if (fps <= 0)
            return nome + "  ---";
        else
            return nome + "  " + QString::number(fps);
    }
    if (tipo == "TEMP") return nome + " " + QString::number(temperatura) + " C";
    return "";
}

// ============================================================
// CONSTRUTOR DA JANELA
// ============================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , timer(new QTimer(this))
    , serial(INVALID_HANDLE_VALUE)
{
    ui->setupUi(this);

    // Estilo Visual Global (Dark Theme Profissional)
    qApp->setStyleSheet(R"(
        QMainWindow {
            background-color: #121619;
            color: #ffffff;
            font-family: 'Segoe UI', sans-serif;
        }
        QGroupBox {
            color: #00d2ff;
            font-weight: bold;
            border: 1px solid #1f2c34;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 15px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
        QLabel {
            color: #cccccc;
            font-size: 13px;
        }
        QComboBox, QLineEdit {
            background-color: #1a2228;
            color: #ffffff;
            border: 1px solid #2d3c46;
            border-radius: 4px;
            padding: 5px;
            selection-background-color: #007acc;
        }
        QComboBox::drop-down {
            border: 0px;
        }
        QPushButton {
            background-color: #007acc;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #0098ff;
        }
        QPushButton:pressed {
            background-color: #005999;
        }
        QCheckBox {
            color: #cccccc;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            background: #1a2228;
            border: 1px solid #2d3c46;
            border-radius: 4px;
        }
        QCheckBox::indicator:checked {
            background: #007acc;
            border: 1px solid #007acc;
        }
    )");

    QStringList opcoes = { "CPU", "RAM", "GPU", "FPS", "TEMP" };

    ui->comboBoxLinha1->clear();
    ui->comboBoxLinha2->clear();
    ui->comboBoxLinha3->clear();
    ui->comboBoxLinha4->clear();

    ui->comboBoxLinha1->addItems(opcoes);
    ui->comboBoxLinha2->addItems(opcoes);
    ui->comboBoxLinha3->addItems(opcoes);
    ui->comboBoxLinha4->addItems(opcoes);

    QSettings settings("ESP32Monitor", "ESP32PCMonitor");

    ui->comboBoxLinha1->setCurrentText(settings.value("linha1_tipo", "CPU").toString());
    ui->comboBoxLinha2->setCurrentText(settings.value("linha2_tipo", "RAM").toString());
    ui->comboBoxLinha3->setCurrentText(settings.value("linha3_tipo", "GPU").toString());
    ui->comboBoxLinha4->setCurrentText(settings.value("linha4_tipo", "FPS").toString());

    // CORRIGIDO: usando setText() em vez de setPlainText() para QLineEdit
    ui->lineEditTexto1->setText(settings.value("linha1_texto", "CPU").toString());
    ui->lineEditTexto2->setText(settings.value("linha2_texto", "RAM").toString());
    ui->lineEditTexto3->setText(settings.value("linha3_texto", "GPU").toString());
    ui->lineEditTexto4->setText(settings.value("linha4_texto", "FPS").toString());

    // Conexões de Botões
    connect(ui->pushButtonAplicar, &QPushButton::clicked, this, &MainWindow::aplicarConfiguracaoDisplay);
    connect(ui->pushButtonRestaurar, &QPushButton::clicked, this, &MainWindow::restaurarConfiguracaoDisplay);

    // Criação da Bandeja do Windows (DEVE vir antes de checar se inicia oculto)
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    trayIcon->setToolTip("ESP32 PC Monitor");

    trayMenu = new QMenu(this);
    QAction *abrir = trayMenu->addAction("Abrir");
    QAction *sair = trayMenu->addAction("Sair");
    trayIcon->setContextMenu(trayMenu);

    connect(abrir, &QAction::triggered, this, &MainWindow::mostrarJanela);
    connect(sair, &QAction::triggered, this, &MainWindow::sairAplicativo);
    connect(trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            mostrarJanela();
        }
    });
    trayIcon->show();



    // Iniciar com o Windows
    QSettings registro("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    ui->checkBoxIniciarPC->setChecked(registro.contains("ESP32 PC Monitor"));
    connect(ui->checkBoxIniciarPC, &QCheckBox::toggled, this, &MainWindow::alterarInicializacao);

    // Abrir ESP32 (Serial)
    serial = abrirSerial();
    if (serial == INVALID_HANDLE_VALUE)
    {
        ui->label->setText("ESP32: ERRO AO CONECTAR");
        return;
    }

    // Iniciar CPU
    if (!iniciarCPU())
    {
        ui->label->setText("ERRO AO INICIAR CPU");
        CloseHandle(serial);
        serial = INVALID_HANDLE_VALUE;
        return;
    }

    // Iniciar ADLX
    if (!iniciarADLX())
    {
        ui->label->setText("ERRO AO INICIAR ADLX");
        finalizarCPU();
        CloseHandle(serial);
        serial = INVALID_HANDLE_VALUE;
        return;
    }

    // Encontrar GPU
    if (!encontrarGPUAMD())
    {
        ui->label->setText("GPU AMD NAO ENCONTRADA");
        finalizarADLX();
        finalizarCPU();
        CloseHandle(serial);
        serial = INVALID_HANDLE_VALUE;
        return;
    }

    // Iniciar Métricas
    if (!iniciarMetricasGPU())
    {
        ui->label->setText("ERRO NAS METRICAS DA GPU");
        finalizarADLX();
        finalizarCPU();
        CloseHandle(serial);
        serial = INVALID_HANDLE_VALUE;
        return;
    }

    // Timer
    connect(timer, &QTimer::timeout, this, &MainWindow::atualizarDados);
    timer->start(1000);

    atualizarDados();
}

// ============================================================
// ATUALIZAR DADOS
// ============================================================

void MainWindow::atualizarDados()
{
    if (serial == INVALID_HANDLE_VALUE)
        return;

    int cpu = getCPUUsage();
    int ram = getRAMUsage();
    int gpu = 0;
    int temperaturaGPU = 0;
    int fps = 0;

    getGPUInfo(gpu, temperaturaGPU, fps);

    cpuAtual = cpu;
    ramAtual = ram;
    gpuAtual = gpu;
    fpsAtual = fps;
    temperaturaAtual = temperaturaGPU;

    enviarDisplay();

    QString texto = QString("CPU: %1%\n"
                            "RAM: %2%\n"
                            "GPU: %3%\n"
                            "TEMP: %4 C\n"
                            "FPS: %5")
                        .arg(cpu)
                        .arg(ram)
                        .arg(gpu)
                        .arg(temperaturaGPU)
                        .arg(fps);

    ui->label->setText(texto);
}

// ============================================================
// MINIMIZAR PARA BANDEJA
// ============================================================

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange)
    {
        if (isMinimized())
        {
            hide();
            trayIcon->show();
        }
    }
    QMainWindow::changeEvent(event);
}

// ============================================================
// MOSTRAR JANELA
// ============================================================

void MainWindow::mostrarJanela()
{
    showNormal();
    raise();
    activateWindow();
}

// ============================================================
// SAIR DO APLICATIVO
// ============================================================

void MainWindow::sairAplicativo()
{
    trayIcon->hide();
    QApplication::quit();
}

// ============================================================
// ENVIAR AS 4 LINHAS PARA O ESP32
// ============================================================

void MainWindow::enviarDisplay()
{
    if (serial == INVALID_HANDLE_VALUE)
        return;

    // CORRIGIDO: usando text() em vez de toPlainText() para QLineEdit
    QString linha1 = gerarTextoLinha(ui->comboBoxLinha1->currentText(), ui->lineEditTexto1->text(), cpuAtual, ramAtual, gpuAtual, fpsAtual, temperaturaAtual);
    QString linha2 = gerarTextoLinha(ui->comboBoxLinha2->currentText(), ui->lineEditTexto2->text(), cpuAtual, ramAtual, gpuAtual, fpsAtual, temperaturaAtual);
    QString linha3 = gerarTextoLinha(ui->comboBoxLinha3->currentText(), ui->lineEditTexto3->text(), cpuAtual, ramAtual, gpuAtual, fpsAtual, temperaturaAtual);
    QString linha4 = gerarTextoLinha(ui->comboBoxLinha4->currentText(), ui->lineEditTexto4->text(), cpuAtual, ramAtual, gpuAtual, fpsAtual, temperaturaAtual);

    QString mensagem = "LINHA1:" + linha1 + "\n" +
                       "LINHA2:" + linha2 + "\n" +
                       "LINHA3:" + linha3 + "\n" +
                       "LINHA4:" + linha4 + "\n";

    QByteArray dados = mensagem.toUtf8();
    DWORD enviados = 0;

    WriteFile(
        serial,
        dados.constData(),
        static_cast<DWORD>(dados.size()),
        &enviados,
        NULL
        );
}

// ============================================================
// APLICAR CONFIGURAÇÃO
// ============================================================

void MainWindow::aplicarConfiguracaoDisplay()
{
    QSettings settings("ESP32Monitor", "ESP32PCMonitor");

    settings.setValue("linha1_tipo", ui->comboBoxLinha1->currentText());
    settings.setValue("linha2_tipo", ui->comboBoxLinha2->currentText());
    settings.setValue("linha3_tipo", ui->comboBoxLinha3->currentText());
    settings.setValue("linha4_tipo", ui->comboBoxLinha4->currentText());

    // CORRIGIDO: usando text() em vez de toPlainText()
    settings.setValue("linha1_texto", ui->lineEditTexto1->text());
    settings.setValue("linha2_texto", ui->lineEditTexto2->text());
    settings.setValue("linha3_texto", ui->lineEditTexto3->text());
    settings.setValue("linha4_texto", ui->lineEditTexto4->text());

    settings.sync();
    enviarDisplay();
}

// ============================================================
// RESTAURAR CONFIGURAÇÃO PADRÃO
// ============================================================

void MainWindow::restaurarConfiguracaoDisplay()
{
    ui->comboBoxLinha1->setCurrentText("CPU");
    ui->comboBoxLinha2->setCurrentText("RAM");
    ui->comboBoxLinha3->setCurrentText("GPU");
    ui->comboBoxLinha4->setCurrentText("FPS");

    // CORRIGIDO: usando setText() em vez de setPlainText()
    ui->lineEditTexto1->setText("CPU");
    ui->lineEditTexto2->setText("RAM");
    ui->lineEditTexto3->setText("GPU");
    ui->lineEditTexto4->setText("FPS");

    enviarDisplay();
}

// ============================================================
// DESTRUTOR
// ============================================================

MainWindow::~MainWindow()
{
    if (timer)
        timer->stop();

    finalizarADLX();
    finalizarCPU();

    if (serial != INVALID_HANDLE_VALUE)
    {
        CloseHandle(serial);
        serial = INVALID_HANDLE_VALUE;
    }

    delete ui;
}
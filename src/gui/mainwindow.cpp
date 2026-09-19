#include "mainwindow.h"
#include "core/converter.h"

#include <QCoreApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPixmap>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <exception>

namespace arena::tes3json {
namespace {

QString compactErrorText(QString error, UiLanguage language)
{
    constexpr qsizetype kMaxErrorChars = 1800;
    error = error.trimmed();
    if (error.size() <= kMaxErrorChars) return error;

    const qsizetype head = 1400;
    const qsizetype tail = 300;
    return error.left(head)
        + l10n(language,
               "\n\n… message truncated …\n\n",
               "\n\n… сообщение сокращено …\n\n")
        + error.right(tail);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_language(systemUiLanguage())
{
    setWindowTitle(QStringLiteral("ArenaTES3JSON %1").arg(QCoreApplication::applicationVersion()));
    setWindowIcon(QIcon(QStringLiteral(":/ArenaTES3JSON.png")));
    resize(660, 240);
    setMinimumSize(560, 220);
    setAcceptDrops(true);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    auto *headerRow = new QHBoxLayout();
    auto *appIcon = new QLabel(central);
    appIcon->setFixedSize(40, 40);
    const QPixmap iconPixmap(QStringLiteral(":/ArenaTES3JSON.png"));
    appIcon->setPixmap(iconPixmap.scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    appIcon->setAlignment(Qt::AlignCenter);

    auto *title = new QLabel(QStringLiteral("<b>ArenaTES3JSON</b>"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    title->setFont(titleFont);
    headerRow->addWidget(appIcon);
    headerRow->addWidget(title);
    headerRow->addStretch(1);
    layout->addLayout(headerRow);

    auto *hint = new QLabel(QStringLiteral("ESM / ESP ↔ JSON • Windows-1251 / 1C"), central);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *inputRow = new QHBoxLayout();
    m_input = new QLineEdit(central);
    m_input->setPlaceholderText(text("Choose .esm, .esp or .json", "Выберите .esm, .esp или .json"));
    m_inputBrowse = new QPushButton(text("Browse…", "Обзор…"), central);
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_inputBrowse);
    layout->addLayout(inputRow);

    auto *outputRow = new QHBoxLayout();
    m_output = new QLineEdit(central);
    m_output->setPlaceholderText(text("Output file is selected automatically", "Выходной файл определяется автоматически"));
    m_outputBrowse = new QPushButton(text("Save…", "Сохранить…"), central);
    outputRow->addWidget(m_output, 1);
    outputRow->addWidget(m_outputBrowse);
    layout->addLayout(outputRow);

    m_direction = new QLabel(text("Direction: —", "Направление: —"), central);
    layout->addWidget(m_direction);

    m_progress = new QProgressBar(central);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(true);
    layout->addWidget(m_progress);

    auto *bottomRow = new QHBoxLayout();
    m_status = new QLabel(text("Ready", "Готово к работе"), central);
    m_status->setWordWrap(true);
    m_convert = new QPushButton(text("Convert", "Конвертировать"), central);
    m_convert->setDefault(true);
    m_convert->setMinimumWidth(150);
    bottomRow->addWidget(m_status, 1);
    bottomRow->addWidget(m_convert);
    layout->addLayout(bottomRow);

    setCentralWidget(central);

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_inputBrowse, &QPushButton::clicked, this, &MainWindow::browseInput);
    connect(m_outputBrowse, &QPushButton::clicked, this, &MainWindow::browseOutput);
    connect(m_input, &QLineEdit::textChanged, this, &MainWindow::updateOutput);
    connect(m_convert, &QPushButton::clicked, this, &MainWindow::convert);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &MainWindow::readBackendStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &MainWindow::readBackendStderr);
    connect(m_process, &QProcess::finished, this, &MainWindow::backendFinished);
    connect(m_process, &QProcess::errorOccurred, this, &MainWindow::backendError);
}

QString MainWindow::text(const char *english, const char *russian) const
{
    return l10n(m_language, english, russian);
}

void MainWindow::browseInput()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        text("Open TES3 plugin or JSON", "Открыть TES3 plugin или JSON"),
        {},
        text("TES3/JSON (*.esm *.esp *.json);;All files (*.*)",
             "TES3/JSON (*.esm *.esp *.json);;Все файлы (*.*)"));
    if (!path.isEmpty()) m_input->setText(path);
}

void MainWindow::browseOutput()
{
    const QString path = QFileDialog::getSaveFileName(
        this,
        text("Save result", "Сохранить результат"),
        m_output->text());
    if (!path.isEmpty()) m_output->setText(path);
}

QString MainWindow::directionText() const
{
    const QString ext = QFileInfo(m_input->text()).suffix().toLower();
    if (ext == QStringLiteral("esm") || ext == QStringLiteral("esp")) {
        return QStringLiteral("ESM/ESP → JSON");
    }
    if (ext == QStringLiteral("json")) {
        return QStringLiteral("JSON → ESM/ESP");
    }
    return QStringLiteral("—");
}

void MainWindow::updateOutput()
{
    if (m_input->text().isEmpty()) {
        m_output->clear();
        m_direction->setText(text("Direction: —", "Направление: —"));
        return;
    }
    m_output->setText(Converter::defaultOutputFor(m_input->text()));
    m_direction->setText(text("Direction: %1", "Направление: %1").arg(directionText()));
}

void MainWindow::setBusy(bool busy)
{
    m_input->setEnabled(!busy);
    m_output->setEnabled(!busy);
    m_inputBrowse->setEnabled(!busy);
    m_outputBrowse->setEnabled(!busy);
    m_convert->setEnabled(!busy);
}

void MainWindow::convert()
{
    if (m_input->text().isEmpty() || m_output->text().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("ArenaTES3JSON"),
                                 text("Choose an input file.", "Выберите входной файл."));
        return;
    }

    const QString ext = QFileInfo(m_input->text()).suffix().toLower();
    QString command;
    if (ext == QStringLiteral("esm") || ext == QStringLiteral("esp")) {
        command = QStringLiteral("to-json");
    } else if (ext == QStringLiteral("json")) {
        command = QStringLiteral("to-plugin");
    } else {
        QMessageBox::warning(this, QStringLiteral("ArenaTES3JSON"),
                             text("Only .esm, .esp and .json are supported.",
                                  "Поддерживаются только .esm, .esp и .json."));
        return;
    }

    m_stdout.clear();
    m_stderrPending.clear();
    m_errorText.clear();
    m_progress->setValue(0);
    m_status->setText(text("Preparing…", "Подготовка…"));
    setBusy(true);

    m_process->setProgram(Converter::backendPath());
    m_process->setArguments({command,
                             m_input->text(),
                             m_output->text(),
                             QStringLiteral("--encoding"),
                             QStringLiteral("cp1251")});
    m_process->start();
}

void MainWindow::readBackendStdout()
{
    m_stdout += m_process->readAllStandardOutput();
}

void MainWindow::handleProgressLine(const QByteArray &line)
{
    if (!line.startsWith("AT3J_PROGRESS\t")) {
        if (!line.trimmed().isEmpty()) {
            m_errorText += line;
            m_errorText += '\n';
        }
        return;
    }

    const QList<QByteArray> parts = line.split('\t');
    if (parts.size() < 2) return;
    bool ok = false;
    const int value = parts.at(1).toInt(&ok);
    if (!ok) return;
    m_progress->setValue(qBound(0, value, 100));
    m_status->setText(text("Converting… %1%", "Конвертация… %1%").arg(value));
}

void MainWindow::readBackendStderr()
{
    m_stderrPending += m_process->readAllStandardError();
    while (true) {
        const qsizetype newline = m_stderrPending.indexOf('\n');
        if (newline < 0) break;
        const QByteArray line = m_stderrPending.left(newline).trimmed();
        m_stderrPending.remove(0, newline + 1);
        handleProgressLine(line);
    }
}

void MainWindow::backendFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    readBackendStdout();
    readBackendStderr();
    if (!m_stderrPending.trimmed().isEmpty()) {
        handleProgressLine(m_stderrPending.trimmed());
        m_stderrPending.clear();
    }

    setBusy(false);

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        m_progress->setValue(0);
        QString error = QString::fromUtf8(m_errorText.trimmed());
        if (error.isEmpty()) error = QString::fromUtf8(m_stdout.trimmed());
        if (error.isEmpty()) error = text("Unknown backend error.", "Неизвестная ошибка backend.");
        m_status->setText(text("Error", "Ошибка"));
        QMessageBox::critical(this,
                              text("ArenaTES3JSON Error", "Ошибка ArenaTES3JSON"),
                              compactErrorText(error, m_language));
        return;
    }

    try {
        const ConversionResult result = Converter::parseBackendStatus(m_stdout);
        m_progress->setValue(100);
        m_status->setText(text("Done • %1 bytes", "Готово • %1 байт").arg(result.outputSize));
    } catch (const std::exception &error) {
        m_progress->setValue(0);
        m_status->setText(text("Status error", "Ошибка статуса"));
        QMessageBox::critical(this,
                              text("ArenaTES3JSON Error", "Ошибка ArenaTES3JSON"),
                              compactErrorText(QString::fromUtf8(error.what()), m_language));
    }
}

void MainWindow::backendError(QProcess::ProcessError error)
{
    if (error != QProcess::FailedToStart) return;
    setBusy(false);
    m_progress->setValue(0);
    m_status->setText(text("Could not start backend", "Не удалось запустить backend"));
    QMessageBox::critical(this,
                          text("ArenaTES3JSON Error", "Ошибка ArenaTES3JSON"),
                          text("Could not start %1\n%2", "Не удалось запустить %1\n%2")
                              .arg(Converter::backendPath(), m_process->errorString()));
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (m_process->state() == QProcess::NotRunning && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (m_process->state() != QProcess::NotRunning) return;
    const auto urls = event->mimeData()->urls();
    if (!urls.isEmpty() && urls.first().isLocalFile()) {
        m_input->setText(urls.first().toLocalFile());
    }
}

} // namespace arena::tes3json

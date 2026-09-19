#include "mainwindow.h"
#include "core/converter.h"

#include <QCheckBox>
#include <QComboBox>
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
#include <QTimer>
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

bool isSupportedInput(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == QStringLiteral("esm") || ext == QStringLiteral("esp")
        || ext == QStringLiteral("json");
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_language(systemUiLanguage())
{
    setWindowTitle(QStringLiteral("ArenaTES3JSON %1").arg(QCoreApplication::applicationVersion()));
    setWindowIcon(QIcon(QStringLiteral(":/ArenaTES3JSON.png")));
    resize(720, 370);
    setMinimumSize(620, 340);
    setAcceptDrops(true);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(9);

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

    auto *hint = new QLabel(
        text("TES3 ESM / ESP ↔ semantic JSON • automatic text encoding",
             "TES3 ESM / ESP ↔ semantic JSON • автоопределение кодировки"),
        central);
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
    m_output->setPlaceholderText(text("Output file is selected automatically",
                                      "Выходной файл определяется автоматически"));
    m_outputBrowse = new QPushButton(text("Save…", "Сохранить…"), central);
    outputRow->addWidget(m_output, 1);
    outputRow->addWidget(m_outputBrowse);
    layout->addLayout(outputRow);

    auto *optionsRow = new QHBoxLayout();
    auto *encodingLabel = new QLabel(text("Encoding:", "Кодировка:"), central);
    m_encoding = new QComboBox(central);
    m_encoding->setEditable(true);
    m_encoding->setInsertPolicy(QComboBox::NoInsert);
    m_encoding->setMinimumWidth(200);
    populateEncodings();
    optionsRow->addWidget(encodingLabel);
    optionsRow->addWidget(m_encoding);
    optionsRow->addSpacing(12);

    m_repairScripts = new QCheckBox(
        text("Repair changed script bytecode", "Ремонт байткода изменённых скриптов"), central);
    m_repairScripts->setChecked(false);
    m_repairScripts->setToolTip(text(
        "For scripts whose SCTX text changed since ArenaTES3JSON export, rebuild SCHD/SCVR and clear stale SCDT, like TES3ZER0EDIT. This prevents old bytecode from being kept; a full vanilla TESCS opcode compiler is not embedded.",
        "Для скриптов, у которых изменился текст SCTX после экспорта ArenaTES3JSON, пересобирает SCHD/SCVR и очищает устаревший SCDT, как TES3ZER0EDIT. Старый байткод не сохраняется; полный компилятор опкодов TESCS внутрь программы не встроен."));
    optionsRow->addWidget(m_repairScripts, 1);
    layout->addLayout(optionsRow);

    m_direction = new QLabel(text("Direction: —", "Направление: —"), central);
    layout->addWidget(m_direction);

    m_fileInfo = new QLabel(text("File: not selected", "Файл: не выбран"), central);
    m_fileInfo->setWordWrap(true);
    m_fileInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_fileInfo);

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
    m_inspectProcess = new QProcess(this);
    m_inspectProcess->setProcessChannelMode(QProcess::SeparateChannels);
    m_inspectTimer = new QTimer(this);
    m_inspectTimer->setSingleShot(true);
    m_inspectTimer->setInterval(250);

    connect(m_inputBrowse, &QPushButton::clicked, this, &MainWindow::browseInput);
    connect(m_outputBrowse, &QPushButton::clicked, this, &MainWindow::browseOutput);
    connect(m_input, &QLineEdit::textChanged, this, &MainWindow::updateForInput);
    connect(m_encoding, &QComboBox::currentTextChanged, this, [this] { scheduleInspection(); });
    connect(m_convert, &QPushButton::clicked, this, &MainWindow::convert);
    connect(m_inspectTimer, &QTimer::timeout, this, &MainWindow::startInspection);
    connect(m_inspectProcess, &QProcess::readyReadStandardOutput, this, [this] {
        m_inspectStdout += m_inspectProcess->readAllStandardOutput();
    });
    connect(m_inspectProcess, &QProcess::readyReadStandardError, this, [this] {
        m_inspectStderr += m_inspectProcess->readAllStandardError();
    });
    connect(m_inspectProcess, &QProcess::finished, this, &MainWindow::inspectionFinished);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &MainWindow::readBackendStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &MainWindow::readBackendStderr);
    connect(m_process, &QProcess::finished, this, &MainWindow::backendFinished);
    connect(m_process, &QProcess::errorOccurred, this, &MainWindow::backendError);

    updateForInput();
}

QString MainWindow::text(const char *english, const char *russian) const
{
    return l10n(m_language, english, russian);
}

void MainWindow::populateEncodings()
{
    m_encoding->clear();
    m_encoding->addItem(text("Auto (recommended)", "Авто (рекомендуется)"), QStringLiteral("auto"));

    // Encoding labels accepted by encoding_rs/WHATWG plus the common aliases
    // used by Morrowind localizations. The combo is editable so any supported
    // alias can also be typed manually.
    const QStringList encodings{
        QStringLiteral("windows-1250"), QStringLiteral("windows-1251"),
        QStringLiteral("windows-1252"), QStringLiteral("windows-1253"),
        QStringLiteral("windows-1254"), QStringLiteral("windows-1255"),
        QStringLiteral("windows-1256"), QStringLiteral("windows-1257"),
        QStringLiteral("windows-1258"), QStringLiteral("windows-874"),
        QStringLiteral("ibm866"), QStringLiteral("koi8-r"), QStringLiteral("koi8-u"),
        QStringLiteral("x-mac-cyrillic"), QStringLiteral("macintosh"),
        QStringLiteral("iso-8859-2"), QStringLiteral("iso-8859-3"),
        QStringLiteral("iso-8859-4"), QStringLiteral("iso-8859-5"),
        QStringLiteral("iso-8859-6"), QStringLiteral("iso-8859-7"),
        QStringLiteral("iso-8859-8"), QStringLiteral("iso-8859-8-i"),
        QStringLiteral("iso-8859-10"), QStringLiteral("iso-8859-13"),
        QStringLiteral("iso-8859-14"), QStringLiteral("iso-8859-15"),
        QStringLiteral("iso-8859-16"), QStringLiteral("shift_jis"),
        QStringLiteral("euc-jp"), QStringLiteral("iso-2022-jp"),
        QStringLiteral("gbk"), QStringLiteral("gb18030"), QStringLiteral("big5"),
        QStringLiteral("euc-kr"), QStringLiteral("utf-8"),
        QStringLiteral("raw")
    };
    for (const QString &encoding : encodings) {
        m_encoding->addItem(encoding, encoding);
    }
    m_encoding->setCurrentIndex(0);
}

QString MainWindow::selectedEncoding() const
{
    const int index = m_encoding->currentIndex();
    if (index >= 0) {
        const QString data = m_encoding->itemData(index).toString();
        if (!data.isEmpty()) return data;
    }
    const QString typed = m_encoding->currentText().trimmed();
    return typed.isEmpty() ? QStringLiteral("auto") : typed;
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
    if (ext == QStringLiteral("esm") || ext == QStringLiteral("esp")) return QStringLiteral("ESM/ESP → JSON");
    if (ext == QStringLiteral("json")) return QStringLiteral("JSON → ESM/ESP");
    return QStringLiteral("—");
}

QString MainWindow::formatFileSize(qint64 bytes) const
{
    constexpr double MiB = 1024.0 * 1024.0;
    if (bytes >= static_cast<qint64>(MiB)) {
        return text("%1 MiB (%2 bytes)", "%1 МиБ (%2 байт)")
            .arg(QString::number(bytes / MiB, 'f', 2), QString::number(bytes));
    }
    return text("%1 bytes", "%1 байт").arg(bytes);
}

void MainWindow::updateForInput()
{
    const QString path = m_input->text().trimmed();
    if (path.isEmpty()) {
        m_output->clear();
        m_direction->setText(text("Direction: —", "Направление: —"));
        m_fileInfo->setText(text("File: not selected", "Файл: не выбран"));
        m_repairScripts->setEnabled(false);
        return;
    }

    m_output->setText(Converter::defaultOutputFor(path));
    m_direction->setText(text("Direction: %1", "Направление: %1").arg(directionText()));
    const bool jsonInput = QFileInfo(path).suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0;
    m_repairScripts->setEnabled(jsonInput);
    scheduleInspection();
}

void MainWindow::scheduleInspection()
{
    if (m_process->state() != QProcess::NotRunning) return;
    const QString path = m_input->text().trimmed();
    if (path.isEmpty() || !QFileInfo::exists(path) || !isSupportedInput(path)) {
        if (!path.isEmpty()) m_fileInfo->setText(text("File information unavailable", "Информация о файле недоступна"));
        return;
    }
    m_fileInfo->setText(text("Reading file information…", "Чтение информации о файле…"));
    m_inspectTimer->start();
}

void MainWindow::startInspection()
{
    if (m_process->state() != QProcess::NotRunning) return;
    const QString path = m_input->text().trimmed();
    if (!QFileInfo::exists(path) || !isSupportedInput(path)) return;

    if (m_inspectProcess->state() != QProcess::NotRunning) {
        m_inspectProcess->kill();
        m_inspectProcess->waitForFinished(200);
    }
    m_inspectStdout.clear();
    m_inspectStderr.clear();
    m_inspectProcess->setProgram(Converter::backendPath());
    m_inspectProcess->setArguments({QStringLiteral("inspect"), path,
                                    QStringLiteral("--encoding"), selectedEncoding()});
    m_inspectProcess->start();
}

void MainWindow::inspectionFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_inspectStdout += m_inspectProcess->readAllStandardOutput();
    m_inspectStderr += m_inspectProcess->readAllStandardError();
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        m_fileInfo->setText(text("File information unavailable • %1",
                                 "Информация о файле недоступна • %1")
                                .arg(QString::fromUtf8(m_inspectStderr).trimmed()));
        return;
    }

    try {
        const InspectionResult info = Converter::parseInspectionStatus(m_inspectStdout);
        m_fileInfo->setText(
            text("File: %1 • %2 • %3 objects • encoding: %4",
                 "Файл: %1 • %2 • %3 объектов • кодировка: %4")
                .arg(info.kind,
                     formatFileSize(info.size),
                     QString::number(info.objects),
                     info.encoding));
    } catch (const std::exception &error) {
        m_fileInfo->setText(text("File information unavailable • %1",
                                 "Информация о файле недоступна • %1")
                                .arg(QString::fromUtf8(error.what())));
    }
}

void MainWindow::setBusy(bool busy)
{
    m_input->setEnabled(!busy);
    m_output->setEnabled(!busy);
    m_encoding->setEnabled(!busy);
    m_repairScripts->setEnabled(!busy && QFileInfo(m_input->text()).suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0);
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

    if (m_inspectProcess->state() != QProcess::NotRunning) m_inspectProcess->kill();
    m_inspectTimer->stop();
    m_stdout.clear();
    m_stderrPending.clear();
    m_errorText.clear();
    m_progress->setValue(0);
    m_status->setText(text("Preparing…", "Подготовка…"));
    setBusy(true);

    QStringList arguments{command, m_input->text(), m_output->text(),
                          QStringLiteral("--encoding"), selectedEncoding()};
    if (command == QStringLiteral("to-plugin") && m_repairScripts->isChecked()) {
        arguments << QStringLiteral("--repair-scripts") << QStringLiteral("changed");
    }

    m_process->setProgram(Converter::backendPath());
    m_process->setArguments(arguments);
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
        QString status = text("Done • %1 • %2", "Готово • %1 • %2")
                             .arg(formatFileSize(result.outputSize), result.encoding);
        if (result.repairedScripts > 0) {
            status += text(" • scripts repaired: %1", " • скриптов исправлено: %1")
                          .arg(result.repairedScripts);
        }
        m_status->setText(status);
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
    if (!urls.isEmpty() && urls.first().isLocalFile()) m_input->setText(urls.first().toLocalFile());
}

} // namespace arena::tes3json

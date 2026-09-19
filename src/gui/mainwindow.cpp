#include "mainwindow.h"
#include "core/converter.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <exception>
#include <stdexcept>

namespace arena::tes3json {

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("ArenaTES3JSON 0.2.1 — ESM/ESP ↔ tes3conv JSON"));
    setWindowIcon(QIcon(QStringLiteral(":/ArenaTES3JSON.svg")));
    resize(790, 540);
    setAcceptDrops(true);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);

    auto *title = new QLabel(QStringLiteral("<b>ArenaTES3JSON</b> — ESM/ESP ↔ semantic JSON"), central);
    auto *hint = new QLabel(
        QStringLiteral("JSON совместим со схемой tes3conv: Header, GameSetting, Class, Npc, Cell, DialogueInfo и т. д. "
                       "Windows-1251/1C преобразуется в нормальный Unicode. Lossless-sidecar не добавляет служебные поля в JSON."),
        central);
    hint->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(hint);

    auto *inputRow = new QHBoxLayout();
    m_input = new QLineEdit(central);
    auto *inputBrowse = new QPushButton(QStringLiteral("Обзор…"), central);
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(inputBrowse);

    auto *outputRow = new QHBoxLayout();
    m_output = new QLineEdit(central);
    auto *outputBrowse = new QPushButton(QStringLiteral("Обзор…"), central);
    outputRow->addWidget(m_output, 1);
    outputRow->addWidget(outputBrowse);

    m_encoding = new QComboBox(central);
    m_encoding->addItem(QStringLiteral("Windows-1251 / 1C (русский)"));
    m_encoding->addItem(QStringLiteral("Без перекодировки (raw)"));

    auto *form = new QFormLayout();
    form->addRow(QStringLiteral("Входной ESM/ESP/JSON:"), inputRow);
    form->addRow(QStringLiteral("Выходной файл:"), outputRow);
    form->addRow(QStringLiteral("Кодировка текста:"), m_encoding);
    layout->addLayout(form);

    m_compact = new QCheckBox(QStringLiteral("Компактный JSON (без отступов)"), central);
    m_lossless = new QCheckBox(QStringLiteral("Lossless round-trip: создавать/использовать .arena-lossless"), central);
    m_lossless->setChecked(true);
    m_lossless->setToolTip(QStringLiteral("Если JSON семантически не менялся, исходный ESP/ESM будет восстановлен byte-for-byte."));
    layout->addWidget(m_compact);
    layout->addWidget(m_lossless);

    auto *buttons = new QHBoxLayout();
    m_convert = new QPushButton(QStringLiteral("Конвертировать"), central);
    auto *verifyButton = new QPushButton(QStringLiteral("Проверить lossless"), central);
    buttons->addWidget(m_convert);
    buttons->addWidget(verifyButton);
    buttons->addStretch(1);
    layout->addLayout(buttons);

    m_log = new QTextEdit(central);
    m_log->setReadOnly(true);
    layout->addWidget(m_log, 1);

    setCentralWidget(central);

    connect(inputBrowse, &QPushButton::clicked, this, &MainWindow::browseInput);
    connect(outputBrowse, &QPushButton::clicked, this, &MainWindow::browseOutput);
    connect(m_input, &QLineEdit::textChanged, this, &MainWindow::updateOutput);
    connect(m_convert, &QPushButton::clicked, this, &MainWindow::convert);
    connect(verifyButton, &QPushButton::clicked, this, &MainWindow::verify);
}

void MainWindow::browseInput()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Открыть TES3 plugin или JSON"),
        {},
        QStringLiteral("TES3/JSON (*.esm *.esp *.json);;Все файлы (*.*)"));
    if (!path.isEmpty()) m_input->setText(path);
}

void MainWindow::browseOutput()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Выходной файл"), m_output->text());
    if (!path.isEmpty()) m_output->setText(path);
}

void MainWindow::updateOutput()
{
    if (!m_input->text().isEmpty()) m_output->setText(Converter::defaultOutputFor(m_input->text()));
}

void MainWindow::convert()
{
    if (m_input->text().isEmpty()) return;
    const auto encoding = m_encoding->currentIndex() == 0
        ? TextEncodingMode::Windows1251 : TextEncodingMode::Raw;

    try {
        const QString ext = QFileInfo(m_input->text()).suffix().toLower();
        if (ext == QStringLiteral("esm") || ext == QStringLiteral("esp")) {
            const auto result = Converter::pluginToJson(m_input->text(), m_output->text(),
                                                        m_compact->isChecked(), encoding, m_lossless->isChecked());
            QString log = QStringLiteral("ESM/ESP → JSON\n%1\n%2 → %3 байт")
                              .arg(result.outputPath)
                              .arg(result.inputSize)
                              .arg(result.outputSize);
            if (!result.sidecarPath.isEmpty()) {
                log += QStringLiteral("\nLossless: %1").arg(result.sidecarPath);
            }
            m_log->append(log + QStringLiteral("\n"));
        } else if (ext == QStringLiteral("json")) {
            const auto result = Converter::jsonToPlugin(m_input->text(), m_output->text(),
                                                        encoding, m_lossless->isChecked());
            QString log = QStringLiteral("JSON → ESM/ESP\n%1\n%2 байт\n%3")
                              .arg(result.outputPath)
                              .arg(result.outputSize)
                              .arg(result.message);
            if (result.byteIdenticalToSource) {
                log += QStringLiteral("\nBYTE-FOR-BYTE: OK");
            }
            m_log->append(log + QStringLiteral("\n"));
        } else {
            throw std::runtime_error("Unsupported extension");
        }
    } catch (const std::exception &error) {
        QMessageBox::critical(this, QStringLiteral("Ошибка"), QString::fromUtf8(error.what()));
    }
}

void MainWindow::verify()
{
    if (m_input->text().isEmpty()) return;
    const QString ext = QFileInfo(m_input->text()).suffix().toLower();
    if (ext != QStringLiteral("esm") && ext != QStringLiteral("esp")) {
        QMessageBox::information(this, QStringLiteral("Проверка"), QStringLiteral("Для проверки выбери исходный .esm или .esp."));
        return;
    }

    const auto encoding = m_encoding->currentIndex() == 0
        ? TextEncodingMode::Windows1251 : TextEncodingMode::Raw;
    QString details;
    const bool ok = Converter::verifyRoundTrip(m_input->text(), &details, encoding);
    m_log->append(details + QStringLiteral("\n"));
    QMessageBox::information(this,
                             ok ? QStringLiteral("Lossless: OK") : QStringLiteral("Lossless: ошибка"),
                             details);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const auto urls = event->mimeData()->urls();
    if (!urls.isEmpty() && urls.first().isLocalFile()) m_input->setText(urls.first().toLocalFile());
}

} // namespace arena::tes3json

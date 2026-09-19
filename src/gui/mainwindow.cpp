#include "mainwindow.h"
#include "core/converter.h"

#include <QCheckBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QIcon>
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
    setWindowTitle(QStringLiteral("ArenaTES3JSON 0.1.0 — ESM/ESP ↔ JSON"));
    setWindowIcon(QIcon(QStringLiteral(":/ArenaTES3JSON.svg")));
    resize(760, 480);
    setAcceptDrops(true);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);

    auto *title = new QLabel(QStringLiteral("<b>ArenaTES3JSON</b> — lossless TES3 plugin converter"), central);
    auto *hint = new QLabel(QStringLiteral("ESM/ESP ↔ JSON • Windows-1251/1C • unknown records are preserved • no tes3conv dependency"), central);
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

    auto *form = new QFormLayout();
    form->addRow(QStringLiteral("Входной ESM/ESP/JSON:"), inputRow);
    form->addRow(QStringLiteral("Выходной файл:"), outputRow);
    layout->addLayout(form);

    m_compact = new QCheckBox(QStringLiteral("Компактный JSON (без отступов)"), central);
    layout->addWidget(m_compact);

    auto *buttons = new QHBoxLayout();
    m_convert = new QPushButton(QStringLiteral("Конвертировать"), central);
    auto *verifyButton = new QPushButton(QStringLiteral("Проверить lossless round-trip"), central);
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
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Открыть TES3 plugin или JSON"), {},
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
    try {
        const QString ext = QFileInfo(m_input->text()).suffix().toLower();
        if (ext == QStringLiteral("esm") || ext == QStringLiteral("esp")) {
            const auto r = Converter::pluginToJson(m_input->text(), m_output->text(), m_compact->isChecked());
            m_log->append(QStringLiteral("ESM/ESP → JSON: %1\n%2 → %3 bytes\nSHA-256: %4")
                          .arg(r.outputPath).arg(r.inputSize).arg(r.outputSize).arg(r.inputSha256));
        } else if (ext == QStringLiteral("json")) {
            const auto r = Converter::jsonToPlugin(m_input->text(), m_output->text());
            m_log->append(QStringLiteral("JSON → ESM/ESP: %1\n%2 bytes\nSHA-256: %3%4")
                          .arg(r.outputPath).arg(r.outputSize).arg(r.outputSha256,
                          r.byteIdenticalToSource ? QStringLiteral("\nСовпадает с исходником byte-for-byte.") : QString()));
        } else {
            throw std::runtime_error("Unsupported extension");
        }
    } catch (const std::exception &e) {
        QMessageBox::critical(this, QStringLiteral("Ошибка"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::verify()
{
    if (m_input->text().isEmpty()) return;
    QString details;
    const bool ok = Converter::verifyRoundTrip(m_input->text(), &details);
    m_log->append(details);
    QMessageBox::information(this, ok ? QStringLiteral("Lossless: OK") : QStringLiteral("Lossless: ошибка"), details);
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

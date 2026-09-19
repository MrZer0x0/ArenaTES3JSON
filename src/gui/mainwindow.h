#pragma once

#include "core/localization.h"

#include <QByteArray>
#include <QMainWindow>
#include <QProcess>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTimer;
class QDragEnterEvent;
class QDropEvent;

namespace arena::tes3json {

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void browseInput();
    void browseOutput();
    void updateForInput();
    void scheduleInspection();
    void startInspection();
    void inspectionFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void convert();
    void readBackendStdout();
    void readBackendStderr();
    void backendFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void backendError(QProcess::ProcessError error);

private:
    void setBusy(bool busy);
    void handleProgressLine(const QByteArray &line);
    void populateEncodings();
    QString selectedEncoding() const;
    QString directionText() const;
    QString formatFileSize(qint64 bytes) const;
    QString text(const char *english, const char *russian) const;

    UiLanguage m_language = UiLanguage::English;
    QLineEdit *m_input = nullptr;
    QLineEdit *m_output = nullptr;
    QComboBox *m_encoding = nullptr;
    QCheckBox *m_repairScripts = nullptr;
    QLabel *m_direction = nullptr;
    QLabel *m_fileInfo = nullptr;
    QLabel *m_status = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_inputBrowse = nullptr;
    QPushButton *m_outputBrowse = nullptr;
    QPushButton *m_convert = nullptr;
    QProcess *m_process = nullptr;
    QProcess *m_inspectProcess = nullptr;
    QTimer *m_inspectTimer = nullptr;

    QByteArray m_stdout;
    QByteArray m_stderrPending;
    QByteArray m_errorText;
    QByteArray m_inspectStdout;
    QByteArray m_inspectStderr;
};

} // namespace arena::tes3json
